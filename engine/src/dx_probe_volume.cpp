#include <dx_probe_volume.hpp>
#ifdef KUKI_HAS_DIRECTX
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <dx_acceleration_structure.hpp>
#include <dx_context.hpp>
#include <dx_pipeline.hpp>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vector_relational.hpp>
#include <hash_utils.hpp>
#include <limits>
#include <profiler.hpp>
#include <random>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <vector>
namespace kuki {
namespace {
constexpr uint32_t AUDIT_SLOTS = 8;
constexpr uint32_t AUDIT_GRID = 32;
constexpr uint32_t AUDIT_GROUP = 4;
constexpr uint32_t REPORT_AFTER_FRAMES = 90;
constexpr float SH_DC_TO_IRRADIANCE = .886227f;
constexpr float CHROMA_SEPARATION = .02f;
/// @brief Probes `CSClassify` looks at per frame, as one in this many. Mirrors `CLASSIFY_STRIDE` in
/// probe_trace.hlsl.
constexpr uint32_t PROBE_CLASSIFY_STRIDE = 16;
/// @brief Threads in each update pass's group. Mirrors `GROUP_SIZE` in probe_update.hlsl.
constexpr uint32_t UPDATE_GROUP = 64;
/// @brief Slots in the counter buffer the update passes keep. Mirrors `COUNTER_*` in probe_update.hlsl.
enum ProbeCounter : uint32_t {
  FreeTop,
  Failed,
  LiveProbes,
  LiveLeaves,
  Deepest,
  Queued,
  Splits,
  Merges,
  CounterCount
};
constexpr uint64_t COUNTER_BYTES = CounterCount * sizeof(int32_t);
/// @brief Bytes one frame's region of the changed-box buffer takes: a minimum and a maximum per box.
constexpr uint64_t BOX_REGION_BYTES = PROBE_MAX_DIRTY_BOXES * 2 * sizeof(glm::vec4);
auto RandomRotation(std::mt19937 &engine) -> glm::mat3 {
  std::uniform_real_distribution<float> distribution(0.f, 1.f);
  const auto u1 = distribution(engine);
  const auto u2 = distribution(engine) * glm::two_pi<float>();
  const auto u3 = distribution(engine) * glm::two_pi<float>();
  const auto a = std::sqrt(1.f - u1);
  const auto b = std::sqrt(u1);
  return glm::mat3_cast(glm::quat(b * std::cos(u3), a * std::sin(u2), a * std::cos(u2), b * std::sin(u3)));
}
constexpr auto UnpackHalf(const uint32_t half) -> float {
  const auto exponent = (half >> 10) & 0x1Fu;
  if (exponent == 0)
    return 0.f;
  return std::bit_cast<float>(((exponent + 127 - 15) << 23) | ((half & 0x3FFu) << 13));
}
/// @brief How far back the mean is wound when something changes, at whatever step it is running at.
///
/// `PROBE_REACTIVE_SAMPLES` fixed the count at nineteen steps' worth. The step is now a setting, so
/// the count has to be derived from it or the two would disagree: what the number means is the
/// weight a fresh estimate lands with after a change, and that weight is a ratio of the two.
auto ReactiveSamples(const IndirectLighting &settings) -> uint32_t {
  return 19u * std::max(settings.meanStep, 1u);
}
/// @brief Hashes the settings the trace itself reads, so a change to one resettles the field.
///
/// Only the ones that reach `DXProbeTraceConstants`. What the shading pass does with the finished
/// field is not in here and must not be: it is hashed, where it needs to be, by the renderer.
auto HashTraceSettings(const IndirectLighting &settings) -> size_t {
  size_t hash{};
  for (const auto value : {settings.probeDepthSharpness, settings.probeDepthRange, settings.rayDistanceScale, settings.hitNormalNudge, settings.relocationMargin, settings.relocationDamping, settings.escapeAgreement, settings.buriedLimit, settings.opaqueThreshold, static_cast<float>(settings.meanStep)})
    hash_combine(hash, std::bit_cast<uint32_t>(value));
  return hash;
}
/// @brief Probe slots a volume of this uniform depth is given to start with.
///
/// Twice the uniform grid's own probes, which is room for the adaptive part to put as many probes
/// again on top before the pool has to grow. The chess board at depth four starts with 9826 slots
/// against the 5492 probes the CPU build used to place over it.
auto InitialPoolSize(const uint32_t uniformDepth) -> uint32_t {
  const auto corners = (1u << uniformDepth) + 1;
  return std::clamp(2 * corners * corners * corners, PROBE_POOL_MINIMUM, PROBE_LATTICE_POINTS);
}
auto GroupsFor(const uint32_t threads) -> uint32_t {
  return (threads + UPDATE_GROUP - 1) / UPDATE_GROUP;
}
/// @brief Every entry point in probe_update.hlsl, compiled together when a volume is first laid down.
///
/// Not left to compile on first use, which is what pipelines here usually do. Some of these passes
/// first run long after the scene opens -- the one that forgets measurements runs on the first frame
/// anything moves, and the one that extends the free list on the first growth -- and compiling one
/// then is a hitch of fifty milliseconds at exactly the moment this volume was written to remove one.
constexpr const char *UPDATE_ENTRY_POINTS[]{"CSReset", "CSBeginFrame", "CSDirty", "CSQueue", "CSMeasure", "CSDecideMerge", "CSDecideSplit", "CSLookup", "CSRelease", "CSAcquire", "CSSeed", "CSRefresh", "CSLeafProbes", "CSExtendFree"};
} // namespace
auto DXProbeVolume::Transition(ID3D12Resource *resource, D3D12_RESOURCE_STATES &state, const D3D12_RESOURCE_STATES target) -> void {
  auto *commandList = owner ? owner->GetCommandList() : nullptr;
  if (!resource || !commandList || state == target)
    return;
  const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, state, target);
  commandList->ResourceBarrier(1, &barrier);
  state = target;
}
auto DXProbeVolume::CreateBuffer(const uint64_t bytes, const char *name) -> ComPtr<ID3D12Resource> {
  auto *device = owner ? owner->GetDevice() : nullptr;
  auto *commandList = owner ? owner->GetCommandList() : nullptr;
  if (!device || !commandList || bytes == 0)
    return {};
  // Created in the common state, which is the only one a buffer is really created in whatever is
  // asked for, and moved out of it explicitly so the state every caller tracks is the true one.
  const auto properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  const auto desc = CD3DX12_RESOURCE_DESC::Buffer(bytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  ComPtr<ID3D12Resource> buffer;
  if (DXFailed(device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buffer)), name))
    return {};
  const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(buffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  commandList->ResourceBarrier(1, &barrier);
  return buffer;
}
auto DXProbeVolume::EnsureFixedBuffers() -> bool {
  if (nodeBuffer && lookupBuffer && cornerBuffer && counterBuffer && queueBuffer && boxData && counterReadback)
    return true;
  auto *device = owner ? owner->GetDevice() : nullptr;
  if (!device)
    return false;
  nodeBuffer = CreateBuffer(static_cast<uint64_t>(PROBE_OCTREE_NODE_COUNT) * sizeof(DXOctreeNode), "CreateCommittedResource for the probe octree");
  lookupBuffer = CreateBuffer(static_cast<uint64_t>(PROBE_LATTICE_CELLS) * PROBE_LATTICE_CELLS * PROBE_LATTICE_CELLS * sizeof(uint32_t), "CreateCommittedResource for the probe lookup grid");
  cornerBuffer = CreateBuffer(static_cast<uint64_t>(PROBE_LATTICE_POINTS) * sizeof(uint32_t), "CreateCommittedResource for the probe lattice");
  counterBuffer = CreateBuffer(COUNTER_BYTES, "CreateCommittedResource for the probe counters");
  queueBuffer = CreateBuffer(static_cast<uint64_t>(PROBE_MEASURE_BUDGET) * sizeof(uint32_t), "CreateCommittedResource for the probe measuring queue");
  nodeState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  lookupState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  if (!nodeBuffer || !lookupBuffer || !cornerBuffer || !counterBuffer || !queueBuffer)
    return false;
  // An upload heap read straight by the GPU rather than staged into a default one. It is a few
  // kilobytes read once a frame by one small dispatch, which is what an upload heap is good for, and
  // a region per frame in flight means nothing written here is ever still being read.
  const auto uploadProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  const auto boxDesc = CD3DX12_RESOURCE_DESC::Buffer(BOX_REGION_BYTES * DX_FRAME_COUNT);
  if (DXFailed(device->CreateCommittedResource(&uploadProperties, D3D12_HEAP_FLAG_NONE, &boxDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&boxBuffer)), "CreateCommittedResource for the probe change boxes"))
    return false;
  void *mapped{};
  const CD3DX12_RANGE noRead(0, 0);
  if (DXFailed(boxBuffer->Map(0, &noRead, &mapped), "Map the probe change boxes"))
    return false;
  boxData = static_cast<uint8_t *>(mapped);
  const auto readbackProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
  const auto readbackDesc = CD3DX12_RESOURCE_DESC::Buffer(COUNTER_BYTES * DX_FRAME_COUNT);
  if (DXFailed(device->CreateCommittedResource(&readbackProperties, D3D12_HEAP_FLAG_NONE, &readbackDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&counterReadback)), "CreateCommittedResource for the probe counter readback"))
    return false;
  readbackGeneration.fill(0);
  return true;
}
auto DXProbeVolume::CreatePool(const uint32_t slots) -> bool {
  if (owner) {
    owner->RetireResource(std::move(probeBuffer));
    owner->RetireResource(std::move(freeBuffer));
  }
  probeBuffer = CreateBuffer(static_cast<uint64_t>(slots) * sizeof(DXProbe), "CreateCommittedResource for the probes");
  freeBuffer = CreateBuffer(static_cast<uint64_t>(slots) * sizeof(uint32_t), "CreateCommittedResource for the probe free list");
  probeState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  capacity = probeBuffer && freeBuffer ? slots : 0;
  return capacity > 0;
}
auto DXProbeVolume::GrowPool(const uint32_t slots) -> bool {
  auto *commandList = owner ? owner->GetCommandList() : nullptr;
  if (!commandList || slots <= capacity)
    return false;
  auto probes = CreateBuffer(static_cast<uint64_t>(slots) * sizeof(DXProbe), "CreateCommittedResource for the grown probes");
  auto free = CreateBuffer(static_cast<uint64_t>(slots) * sizeof(uint32_t), "CreateCommittedResource for the grown probe free list");
  if (!probes || !free)
    return false;
  // Copied on the GPU, in the frame's own command list, rather than read back and uploaded: the
  // probes in use are exactly where the passes left them and the copy lands before the passes run.
  auto freeState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  auto newState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  Transition(probeBuffer.Get(), probeState, D3D12_RESOURCE_STATE_COPY_SOURCE);
  Transition(freeBuffer.Get(), freeState, D3D12_RESOURCE_STATE_COPY_SOURCE);
  Transition(probes.Get(), newState, D3D12_RESOURCE_STATE_COPY_DEST);
  newState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  Transition(free.Get(), newState, D3D12_RESOURCE_STATE_COPY_DEST);
  commandList->CopyBufferRegion(probes.Get(), 0, probeBuffer.Get(), 0, static_cast<uint64_t>(capacity) * sizeof(DXProbe));
  commandList->CopyBufferRegion(free.Get(), 0, freeBuffer.Get(), 0, static_cast<uint64_t>(capacity) * sizeof(uint32_t));
  auto copiedState = D3D12_RESOURCE_STATE_COPY_DEST;
  Transition(probes.Get(), copiedState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  copiedState = D3D12_RESOURCE_STATE_COPY_DEST;
  Transition(free.Get(), copiedState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  owner->RetireResource(std::move(probeBuffer));
  owner->RetireResource(std::move(freeBuffer));
  probeBuffer = std::move(probes);
  freeBuffer = std::move(free);
  probeState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  spdlog::info("[DX12] Probe volume: grew the probe pool from {} to {} slots", capacity, slots);
  grownFrom = capacity;
  capacity = slots;
  // Counters already on their way back describe the pool before this, and the shortage they report
  // is the one being answered here.
  ++generation;
  return true;
}
auto DXProbeVolume::TrackGeometry(const std::vector<DXProbeGeometry> &geometry, glm::vec3 &low, glm::vec3 &high) -> bool {
  ++geometryFrame;
  low = glm::vec3(std::numeric_limits<float>::max());
  high = glm::vec3(std::numeric_limits<float>::lowest());
  auto any = false;
  const auto Dirty = [this](const glm::vec3 &from, const glm::vec3 &to) {
    dirtyBoxes.emplace_back(from, 0.f);
    dirtyBoxes.emplace_back(to, 0.f);
  };
  for (const auto &item : geometry) {
    if (item.vertices.empty())
      continue;
    const auto *mesh = static_cast<const void *>(item.vertices.data());
    auto bounds = meshBounds.find(mesh);
    if (bounds == meshBounds.end() || bounds->second.vertexCount != item.vertices.size()) {
      MeshBounds local{.low = item.vertices.front().position, .high = item.vertices.front().position, .vertexCount = item.vertices.size()};
      for (const auto &vertex : item.vertices) {
        local.low = glm::min(local.low, vertex.position);
        local.high = glm::max(local.high, vertex.position);
      }
      bounds = meshBounds.insert_or_assign(mesh, local).first;
    }
    // Eight corners through the matrix and a fresh box around them: conservative under rotation,
    // which only ever asks for a little more re-measuring than was needed.
    auto worldLow = glm::vec3(std::numeric_limits<float>::max());
    auto worldHigh = glm::vec3(std::numeric_limits<float>::lowest());
    for (uint32_t corner = 0; corner < 8; ++corner) {
      const glm::vec3 local{(corner & 1) ? bounds->second.high.x : bounds->second.low.x, (corner & 2) ? bounds->second.high.y : bounds->second.low.y, (corner & 4) ? bounds->second.high.z : bounds->second.low.z};
      const auto world = glm::vec3(item.transform * glm::vec4(local, 1.f));
      worldLow = glm::min(worldLow, world);
      worldHigh = glm::max(worldHigh, world);
    }
    low = glm::min(low, worldLow);
    high = glm::max(high, worldHigh);
    any = true;
    auto [placement, added] = placements.try_emplace(item.entity);
    auto &placed = placement->second;
    if (added)
      Dirty(worldLow, worldHigh);
    else if (placed.mesh != mesh || placed.transform != item.transform) {
      // Where it was and where it is, as two boxes rather than one around both: a piece moved across
      // the board would otherwise re-measure every square it passed over.
      Dirty(placed.low, placed.high);
      Dirty(worldLow, worldHigh);
    }
    placed = Placement{.mesh = mesh, .transform = item.transform, .low = worldLow, .high = worldHigh, .seen = geometryFrame};
  }
  for (auto it = placements.begin(); it != placements.end();) {
    if (it->second.seen == geometryFrame) {
      ++it;
      continue;
    }
    Dirty(it->second.low, it->second.high);
    it = placements.erase(it);
  }
  return any;
}
auto DXProbeVolume::ReadCounters(DXContext &context) -> void {
  // The frame that last recorded into this slot has finished: `BeginFrame` waited on its fence
  // before the slot was handed out again. So this read costs nothing, and is never the stall a
  // synchronous readback would be -- it is simply a few frames old, which nothing here minds.
  const auto slot = context.GetFrameIndex() % DX_FRAME_COUNT;
  if (!counterReadback || readbackGeneration[slot] != generation)
    return;
  readbackGeneration[slot] = 0;
  const CD3DX12_RANGE range(slot * COUNTER_BYTES, (slot + 1) * COUNTER_BYTES);
  void *mapped{};
  if (DXFailed(counterReadback->Map(0, &range, &mapped), "Map the probe counter readback"))
    return;
  int32_t counters[CounterCount]{};
  memcpy(counters, static_cast<const uint8_t *>(mapped) + slot * COUNTER_BYTES, COUNTER_BYTES);
  const CD3DX12_RANGE noWrite(0, 0);
  counterReadback->Unmap(0, &noWrite);
  liveProbes = static_cast<uint32_t>(std::max(counters[LiveProbes], 0));
  leafCount = static_cast<uint32_t>(std::max(counters[LiveLeaves], 0));
  deepestLeaf = static_cast<uint32_t>(std::max(counters[Deepest], 0));
  settled = counters[Queued] == 0 && counters[Splits] == 0 && counters[Merges] == 0 && counters[Failed] == 0;
  if (counters[Splits] > 0 || counters[Merges] > 0)
    spdlog::debug("[DX12] Probe volume: {} leaves split and {} merged, {} nodes measured", counters[Splits], counters[Merges], counters[Queued]);
  if (settled && (liveProbes != loggedProbes || leafCount != loggedLeaves)) {
    loggedProbes = liveProbes;
    loggedLeaves = leafCount;
    const auto bytes = static_cast<uint64_t>(capacity) * sizeof(DXProbe) + static_cast<uint64_t>(PROBE_OCTREE_NODE_COUNT) * sizeof(DXOctreeNode);
    spdlog::info("[DX12] Probe volume: settled at {} probes in {} leaves, deepest {} of {}, over {:.2f} units; {} probe slots, {:.2f} MB resident", liveProbes, leafCount, deepestLeaf, PROBE_OCTREE_MAX_DEPTH, side, capacity, static_cast<double>(bytes) / (1024. * 1024.));
  }
  // Grown on a failure, or when the free list is nearly spent, so the next burst of splits finds
  // room rather than failing first. Doubling keeps the number of growths over a scene's life to a
  // handful; the lattice is the ceiling, since there is nowhere for a probe past it to stand.
  const auto spare = std::max(counters[FreeTop], 0);
  if ((counters[Failed] > 0 || static_cast<uint32_t>(spare) < capacity / 16) && capacity < PROBE_LATTICE_POINTS)
    GrowPool(std::min(capacity * 2, PROBE_LATTICE_POINTS));
}
auto DXProbeVolume::RecordPasses(DXContext &context, DXPipelineCache &pipelines, const DXAccelerationStructure &scene) -> bool {
  KUKI_PROFILE_SCOPE("ProbeVolume::RecordPasses");
  auto *commandList = context.GetCommandList();
  auto *device = context.GetDevice();
  const auto *layout = pipelines.GetProbeUpdatePipeline(device, "CSBeginFrame");
  if (!commandList || !layout || !*layout)
    return false;
  Transition(nodeBuffer.Get(), nodeState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  Transition(lookupBuffer.Get(), lookupState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  Transition(probeBuffer.Get(), probeState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  const auto slot = context.GetFrameIndex() % DX_FRAME_COUNT;
  auto boxCount = static_cast<uint32_t>(dirtyBoxes.size() / 2);
  if (boxCount > PROBE_MAX_DIRTY_BOXES) {
    auto low = glm::vec4(std::numeric_limits<float>::max());
    auto high = glm::vec4(std::numeric_limits<float>::lowest());
    for (size_t box = 0; box < dirtyBoxes.size(); box += 2) {
      low = glm::min(low, dirtyBoxes[box]);
      high = glm::max(high, dirtyBoxes[box + 1]);
    }
    dirtyBoxes = {low, high};
    boxCount = 1;
  }
  if (boxCount > 0)
    memcpy(boxData + slot * BOX_REGION_BYTES, dirtyBoxes.data(), boxCount * 2 * sizeof(glm::vec4));
  dirtyBoxes.clear();
  DXProbeUpdateConstants constants{};
  constants.volume[0] = origin.x;
  constants.volume[1] = origin.y;
  constants.volume[2] = origin.z;
  constants.volume[3] = side;
  constants.counts[0] = capacity;
  constants.counts[1] = uniformDepth;
  constants.counts[2] = boxCount;
  constants.counts[3] = ++frameSeed;
  constants.limits[0] = grownFrom;
  constants.limits[1] = PROBE_MEASURE_BUDGET;
  constants.limits[2] = PROBE_MEASURE_LINES;
  constants.limits[3] = PROBE_LATTICE_CELLS >> uniformDepth;
  constants.tuning[0] = PROBE_OCTREE_LEAF_SURFACE;
  constants.tuning[1] = PROBE_SURFACE_HYSTERESIS;
  constants.tuning[2] = PROBE_RELOCATION_LIMIT;
  constants.tuning[3] = PROBE_DEPTH_RANGE * side;
  const auto boxAddress = boxBuffer->GetGPUVirtualAddress() + slot * BOX_REGION_BYTES;
  commandList->SetComputeRootSignature(layout->rootSignature.Get());
  commandList->SetComputeRoot32BitConstants(0, sizeof(DXProbeUpdateConstants) / sizeof(uint32_t), &constants, 0);
  // A root descriptor has to point somewhere valid whether or not the pass reads it. Only the
  // measuring pass reads the scene, and it is not recorded without one.
  commandList->SetComputeRootShaderResourceView(1, scene.IsReady() ? scene.GetTopLevelAddress() : boxAddress);
  commandList->SetComputeRootShaderResourceView(2, boxAddress);
  commandList->SetComputeRootUnorderedAccessView(3, nodeBuffer->GetGPUVirtualAddress());
  commandList->SetComputeRootUnorderedAccessView(4, probeBuffer->GetGPUVirtualAddress());
  commandList->SetComputeRootUnorderedAccessView(5, lookupBuffer->GetGPUVirtualAddress());
  commandList->SetComputeRootUnorderedAccessView(6, cornerBuffer->GetGPUVirtualAddress());
  commandList->SetComputeRootUnorderedAccessView(7, freeBuffer->GetGPUVirtualAddress());
  commandList->SetComputeRootUnorderedAccessView(8, counterBuffer->GetGPUVirtualAddress());
  commandList->SetComputeRootUnorderedAccessView(9, queueBuffer->GetGPUVirtualAddress());
  // Every pass reads what the one before it wrote, so each is followed by a barrier over all of
  // them. The root arguments outlive a pipeline change under the same root signature, so they are
  // bound once above and only the pipeline changes from here.
  const auto Run = [&](const char *entryPoint, const uint32_t groups) {
    const auto *pipeline = pipelines.GetProbeUpdatePipeline(device, entryPoint);
    if (!pipeline || !*pipeline)
      return false;
    commandList->SetPipelineState(pipeline->pipelineState.Get());
    commandList->Dispatch(groups, 1, 1);
    const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
    commandList->ResourceBarrier(1, &barrier);
    return true;
  };
  auto recorded = true;
  if (resetPending) {
    recorded &= Run("CSReset", GroupsFor(std::max({PROBE_OCTREE_NODE_COUNT, PROBE_LATTICE_POINTS, capacity})));
    resetPending = false;
  }
  if (grownFrom > 0) {
    recorded &= Run("CSExtendFree", GroupsFor(capacity - grownFrom));
    grownFrom = 0;
  }
  recorded &= Run("CSBeginFrame", 1);
  if (boxCount > 0)
    recorded &= Run("CSDirty", GroupsFor(PROBE_OCTREE_NODE_COUNT));
  recorded &= Run("CSQueue", GroupsFor(PROBE_OCTREE_NODE_COUNT));
  // Without an acceleration structure there is nothing to measure against, so nothing settles and
  // nothing splits; the tree stays as it is until there is.
  if (scene.IsReady())
    recorded &= Run("CSMeasure", PROBE_MEASURE_BUDGET);
  recorded &= Run("CSDecideMerge", GroupsFor(PROBE_OCTREE_NODE_COUNT));
  recorded &= Run("CSDecideSplit", GroupsFor(PROBE_OCTREE_NODE_COUNT));
  recorded &= Run("CSLookup", GroupsFor(PROBE_LATTICE_CELLS * PROBE_LATTICE_CELLS * PROBE_LATTICE_CELLS));
  recorded &= Run("CSRelease", GroupsFor(PROBE_LATTICE_POINTS));
  recorded &= Run("CSAcquire", GroupsFor(PROBE_LATTICE_POINTS));
  recorded &= Run("CSSeed", GroupsFor(PROBE_LATTICE_POINTS));
  recorded &= Run("CSRefresh", GroupsFor(PROBE_LATTICE_POINTS));
  recorded &= Run("CSLeafProbes", GroupsFor(PROBE_OCTREE_NODE_COUNT));
  Transition(nodeBuffer.Get(), nodeState, PROBE_READ_STATE);
  Transition(lookupBuffer.Get(), lookupState, PROBE_READ_STATE);
  Transition(probeBuffer.Get(), probeState, PROBE_READ_STATE);
  auto counterState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  Transition(counterBuffer.Get(), counterState, D3D12_RESOURCE_STATE_COPY_SOURCE);
  commandList->CopyBufferRegion(counterReadback.Get(), slot * COUNTER_BYTES, counterBuffer.Get(), 0, COUNTER_BYTES);
  Transition(counterBuffer.Get(), counterState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  readbackGeneration[slot] = generation;
  return recorded;
}
auto DXProbeVolume::Update(DXContext &context, DXPipelineCache &pipelines, const DXAccelerationStructure &scene, const std::vector<DXProbeGeometry> &geometry) -> bool {
  KUKI_PROFILE_SCOPE("ProbeVolume::Update");
  owner = &context;
  if (!context.GetCommandList())
    return ready;
  glm::vec3 low;
  glm::vec3 high;
  if (!TrackGeometry(geometry, low, high)) {
    // Nothing to place probes around. A volume already laid down is left as it stands rather than
    // torn down, since an empty frame is far more often a scene between loads than a scene that
    // has become empty for good.
    dirtyBoxes.clear();
    return ready;
  }
  const auto span = high - low;
  const auto required = std::max({span.x, span.y, span.z, .01f}) * .5f * 1.02f;
  // The volume keeps the frame of reference it already had, for as long as the scene still fits
  // inside it and has not shrunk so far that the resolution is being spent on empty space.
  //
  // Sized to the exact bounding box, it moved whenever anything did. A probe's anchor is measured
  // from the origin, so an origin that shifts by any amount at all moves every probe in the volume,
  // and the whole field starts over -- everywhere rather than near whatever moved. A piece lifting a
  // quarter of a unit was enough to do it, by raising the top of the scene's box.
  //
  // Holding the cube still instead means the lattice lands on the same world positions frame after
  // frame, which is what lets the tree change around a probe without the probe changing. It is also
  // the honest reading of what the volume is: a region of space being sampled, not a fit to whatever
  // happens to be standing in it this frame.
  const auto Inside = [&](const glm::vec3 &point) {
    return glm::all(glm::greaterThanEqual(point, origin)) && glm::all(glm::lessThanEqual(point, origin + glm::vec3(side)));
  };
  const auto keep = ready && side > .0f && Inside(low) && Inside(high) && required * 2.f >= side * PROBE_VOLUME_KEEP_FRACTION;
  if (!keep) {
    KUKI_PROFILE_SCOPE("ProbeVolume::LayDown");
    const auto center = (low + high) * .5f;
    origin = center - glm::vec3(required);
    side = required * 2.f;
    uniformDepth = ProbeUniformDepth(side);
    if (!EnsureFixedBuffers())
      return ready = false;
    for (const auto *entryPoint : UPDATE_ENTRY_POINTS)
      if (const auto *pipeline = pipelines.GetProbeUpdatePipeline(context.GetDevice(), entryPoint); !pipeline || !*pipeline)
        return ready = false;
    if (const auto wanted = InitialPoolSize(uniformDepth); capacity < wanted && !CreatePool(wanted))
      return ready = false;
    resetPending = true;
    grownFrom = 0;
    ++generation;
    // The reset re-measures everything, so a box naming part of it is redundant.
    dirtyBoxes.clear();
    settled = false;
    loggedProbes = 0;
    loggedLeaves = 0;
    validated = false;
    reported = false;
    tracedFrames = 0;
    tracedSamples = 0;
    traceHash = 0;
    spdlog::info("[DX12] Probe volume: laid down over {:.2f} units, uniform to depth {} for {:.2f} units between probes against a target of {:.2f}, {} probe slots", side, uniformDepth, side / static_cast<float>(1u << uniformDepth), PROBE_TARGET_SPACING, capacity);
  } else
    ReadCounters(context);
  if (!RecordPasses(context, pipelines, scene))
    return ready = false;
  return ready = true;
}
auto DXProbeVolume::EnsureAuditBuffers() -> bool {
  if (auditSeedData && auditResult && auditReadback)
    return true;
  auto *device = owner ? owner->GetDevice() : nullptr;
  if (!device)
    return false;
  constexpr uint64_t BYTES = AUDIT_SLOTS * sizeof(uint32_t);
  const auto uploadProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  const auto defaultProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  const auto readbackProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
  auto plain = CD3DX12_RESOURCE_DESC::Buffer(BYTES);
  auto writable = CD3DX12_RESOURCE_DESC::Buffer(BYTES, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  if (DXFailed(device->CreateCommittedResource(&uploadProperties, D3D12_HEAP_FLAG_NONE, &plain, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&auditSeed)), "CreateCommittedResource for the probe audit seed"))
    return false;
  if (DXFailed(device->CreateCommittedResource(&defaultProperties, D3D12_HEAP_FLAG_NONE, &writable, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&auditResult)), "CreateCommittedResource for the probe audit result"))
    return false;
  if (DXFailed(device->CreateCommittedResource(&readbackProperties, D3D12_HEAP_FLAG_NONE, &plain, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&auditReadback)), "CreateCommittedResource for the probe audit readback"))
    return false;
  void *mapped{};
  const CD3DX12_RANGE readRange(0, 0);
  if (DXFailed(auditSeed->Map(0, &readRange, &mapped), "Map the probe audit seed"))
    return false;
  auditSeedData = static_cast<uint8_t *>(mapped);
  return true;
}
auto DXProbeVolume::Validate(DXContext &context, DXPipelineCache &pipelines) -> void {
  // Not until the tree has settled after being laid down: a tree still growing has leaves whose
  // corners are being allocated this frame, and would be reported as broken for being unfinished.
  if (validated || !ready || !settled)
    return;
  validated = true;
  auto *commandList = context.GetCommandList();
  const auto pipeline = pipelines.GetProbeAuditPipeline(context.GetDevice());
  if (!commandList || !pipeline || !*pipeline || !EnsureAuditBuffers())
    return;
  uint32_t seed[AUDIT_SLOTS]{};
  seed[6] = PROBE_OCTREE_MAX_DEPTH;
  memcpy(auditSeedData, seed, sizeof(seed));
  commandList->CopyBufferRegion(auditResult.Get(), 0, auditSeed.Get(), 0, sizeof(seed));
  auto toWrite = CD3DX12_RESOURCE_BARRIER::Transition(auditResult.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  commandList->ResourceBarrier(1, &toWrite);
  DXProbeVolumeConstants constants{};
  constants.origin[0] = origin.x;
  constants.origin[1] = origin.y;
  constants.origin[2] = origin.z;
  constants.origin[3] = side;
  constants.grid[0] = GetLookupResolution();
  constants.grid[1] = AUDIT_GRID;
  constants.grid[2] = capacity;
  constants.grid[3] = PROBE_OCTREE_NODE_COUNT;
  Transition(probeBuffer.Get(), probeState, PROBE_READ_STATE);
  commandList->SetComputeRootSignature(pipeline->rootSignature.Get());
  commandList->SetPipelineState(pipeline->pipelineState.Get());
  commandList->SetComputeRoot32BitConstants(0, sizeof(DXProbeVolumeConstants) / sizeof(uint32_t), &constants, 0);
  commandList->SetComputeRootShaderResourceView(1, nodeBuffer->GetGPUVirtualAddress());
  commandList->SetComputeRootShaderResourceView(2, probeBuffer->GetGPUVirtualAddress());
  commandList->SetComputeRootShaderResourceView(3, lookupBuffer->GetGPUVirtualAddress());
  commandList->SetComputeRootUnorderedAccessView(4, auditResult->GetGPUVirtualAddress());
  commandList->Dispatch(AUDIT_GRID / AUDIT_GROUP, AUDIT_GRID / AUDIT_GROUP, AUDIT_GRID / AUDIT_GROUP);
  auto toRead = CD3DX12_RESOURCE_BARRIER::Transition(auditResult.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
  commandList->ResourceBarrier(1, &toRead);
  commandList->CopyBufferRegion(auditReadback.Get(), 0, auditResult.Get(), 0, sizeof(seed));
  auto toSeed = CD3DX12_RESOURCE_BARRIER::Transition(auditResult.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
  commandList->ResourceBarrier(1, &toSeed);
  context.FlushCommandList();
  uint32_t *mapped{};
  const CD3DX12_RANGE readRange(0, sizeof(seed));
  if (DXFailed(auditReadback->Map(0, &readRange, reinterpret_cast<void **>(&mapped)), "Map the probe audit readback"))
    return;
  uint32_t result[AUDIT_SLOTS]{};
  memcpy(result, mapped, sizeof(result));
  const CD3DX12_RANGE writeRange(0, 0);
  auditReadback->Unmap(0, &writeRange);
  const auto sampled = AUDIT_GRID * AUDIT_GRID * AUDIT_GRID;
  spdlog::info("[DX12] Probe volume: audited {} points, resolving to leaves at depths {} to {}, {} fully consistent", sampled, result[6], result[5], result[7]);
  if (result[1] == 0 && result[2] == 0 && result[3] == 0 && result[4] == 0) {
    spdlog::info("[DX12] Probe volume: every point resolved to a leaf containing it, with probes on its corners");
    return;
  }
  spdlog::error("[DX12] Probe volume: {} points reached no leaf, {} landed outside the leaf they resolved to, {} found an invalid probe index, {} found a probe off its corner", result[1], result[2], result[3], result[4]);
}
auto DXProbeVolume::Trace(DXContext &context, DXPipelineCache &pipelines, const DXAccelerationStructure &scene, const D3D12_GPU_VIRTUAL_ADDRESS frameConstants, const D3D12_GPU_VIRTUAL_ADDRESS skyHarmonics, const size_t sceneHash, const IndirectLighting &settings) -> void {
  KUKI_PROFILE_SCOPE("ProbeVolume::Trace");
  if (!ready || !scene.IsReady() || !frameConstants)
    return;
  owner = &context;
  auto *commandList = context.GetCommandList();
  auto &heap = context.GetSRVHeap();
  const auto pipeline = pipelines.GetProbeTracePipeline(context.GetDevice());
  if (!commandList || !pipeline || !*pipeline || !heap.Get())
    return;
  if (probeState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(probeBuffer.Get(), probeState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    commandList->ResourceBarrier(1, &barrier);
    probeState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  }
  // Folded in here rather than by the caller, because these are the values this function is about
  // to put into the dispatch and it is the only place that knows which they are. Every one of them
  // changes what a ray measures or where a probe stands, so an estimate taken before a change and
  // one taken after are not estimates of the same thing and must not be averaged together.
  auto settled = sceneHash;
  hash_combine(settled, HashTraceSettings(settings));
  // a change does not throw the mean away, it only stops it from being old enough to be rigid
  if (settled != traceHash) {
    traceHash = settled;
    tracedSamples = std::min(tracedSamples, ReactiveSamples(settings));
  }
  const auto rotation = RandomRotation(random);
  DXProbeTraceConstants constants{};
  for (auto row = 0; row < 3; ++row)
    for (auto column = 0; column < 3; ++column)
      constants.rotation[row][column] = rotation[column][row];
  constants.volume[0] = origin.x;
  constants.volume[1] = origin.y;
  constants.volume[2] = origin.z;
  constants.volume[3] = side;
  constants.counts[0] = capacity;
  constants.counts[1] = PROBE_OCTREE_NODE_COUNT;
  // Which sixteenth of the pool `CSClassify` looks at this frame. A frame count rather than the sample
  // count, which is wound back on every change and would ask the same probes twice running.
  constants.counts[2] = tracedFrames;
  constants.counts[3] = GetLookupResolution();
  // how much of the mean survives: nothing accumulated yet means the estimate is taken whole
  const auto step = std::max(settings.meanStep, 1u);
  constants.params[0] = static_cast<float>(tracedSamples) / static_cast<float>(tracedSamples + step);
  constants.params[1] = side * settings.rayDistanceScale;
  constants.params[2] = side / static_cast<float>(GetLookupResolution()) * settings.hitNormalNudge;
  constants.params[3] = settings.probeDepthSharpness;
  constants.relocation[0] = settings.relocationMargin;
  constants.relocation[1] = settings.relocationDamping;
  constants.relocation[2] = settings.escapeAgreement;
  constants.relocation[3] = settings.buriedLimit;
  constants.field[0] = settings.probeDepthRange;
  constants.field[1] = settings.opaqueThreshold;
  commandList->SetComputeRootSignature(pipeline->rootSignature.Get());
  commandList->SetPipelineState(pipeline->pipelineState.Get());
  commandList->SetComputeRoot32BitConstants(0, sizeof(DXProbeTraceConstants) / sizeof(uint32_t), &constants, 0);
  commandList->SetComputeRootConstantBufferView(1, frameConstants);
  commandList->SetComputeRootShaderResourceView(2, scene.GetTopLevelAddress());
  commandList->SetComputeRootShaderResourceView(3, scene.GetGeometryInfoAddress());
  commandList->SetComputeRootShaderResourceView(4, nodeBuffer->GetGPUVirtualAddress());
  commandList->SetComputeRootShaderResourceView(5, lookupBuffer->GetGPUVirtualAddress());
  commandList->SetComputeRootUnorderedAccessView(6, probeBuffer->GetGPUVirtualAddress());
  commandList->SetComputeRootDescriptorTable(7, heap.GetGPUHandle(0));
  commandList->SetComputeRootDescriptorTable(8, heap.GetGPUHandle(0));
  // A root descriptor has to point somewhere valid whether or not the shader looks at it, and with
  // no sky projected there is nothing to point at. The probes themselves stand in: the trace reads
  // this only when the frame says a skybox has been prepared, which is exactly when the real buffer
  // is there.
  commandList->SetComputeRootShaderResourceView(9, skyHarmonics ? skyHarmonics : probeBuffer->GetGPUVirtualAddress());
  // The whole pool, since which slots hold a probe is decided on the GPU this frame. See the trace.
  commandList->Dispatch(capacity, 1, 1);
  // Under the same root signature and arguments, so only the pipeline changes. After the trace rather
  // than before it so the two never write the same probe's record in one dispatch.
  if (const auto classify = pipelines.GetProbeTracePipeline(context.GetDevice(), "CSClassify"); classify && *classify) {
    const auto ordered = CD3DX12_RESOURCE_BARRIER::UAV(probeBuffer.Get());
    commandList->ResourceBarrier(1, &ordered);
    commandList->SetPipelineState(classify->pipelineState.Get());
    commandList->Dispatch((capacity + PROBE_CLASSIFY_STRIDE - 1) / PROBE_CLASSIFY_STRIDE, 1, 1);
  }
  const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(probeBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, PROBE_READ_STATE);
  commandList->ResourceBarrier(1, &barrier);
  probeState = PROBE_READ_STATE;
  ++tracedFrames;
  ++tracedSamples;
}
auto DXProbeVolume::Report(DXContext &context) -> void {
  if (reported || !ready || !settled || tracedFrames < REPORT_AFTER_FRAMES)
    return;
  reported = true;
  auto *device = context.GetDevice();
  auto *commandList = context.GetCommandList();
  if (!device || !commandList)
    return;
  const auto bytes = static_cast<uint64_t>(capacity) * sizeof(DXProbe);
  const auto readbackProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
  auto desc = CD3DX12_RESOURCE_DESC::Buffer(bytes);
  if (DXFailed(device->CreateCommittedResource(&readbackProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&probeReadback)), "CreateCommittedResource for the probe readback"))
    return;
  const auto toSource = CD3DX12_RESOURCE_BARRIER::Transition(probeBuffer.Get(), probeState, D3D12_RESOURCE_STATE_COPY_SOURCE);
  commandList->ResourceBarrier(1, &toSource);
  commandList->CopyBufferRegion(probeReadback.Get(), 0, probeBuffer.Get(), 0, bytes);
  const auto toPrevious = CD3DX12_RESOURCE_BARRIER::Transition(probeBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, probeState);
  commandList->ResourceBarrier(1, &toPrevious);
  context.FlushCommandList();
  const DXProbe *probes{};
  const CD3DX12_RANGE readRange(0, static_cast<SIZE_T>(bytes));
  if (DXFailed(probeReadback->Map(0, &readRange, reinterpret_cast<void **>(const_cast<DXProbe **>(&probes))), "Map the probe readback"))
    return;
  glm::vec3 total{};
  glm::vec3 lower{};
  glm::vec3 upper{};
  auto lit = 0u;
  auto moved = 0u;
  auto filled = 0u;
  auto displacement = 0.;
  auto lowerCount = 0u;
  auto upperCount = 0u;
  auto reach = 0.;
  auto deviation = 0.;
  auto behind = 0.;
  auto backfacing = 0u;
  auto shell = 0u;
  auto shellMean = 0.;
  auto shellSquare = 0.;
  auto coreMean = 0.;
  auto coreSquare = 0.;
  auto coreCount = 0u;
  auto open = 0u;
  auto outside = 0u;
  auto brightest = 0.f;
  const auto middle = origin.x + side * .5f;
  const auto range = PROBE_DEPTH_RANGE * side;
  auto probeCount = 0u;
  for (uint32_t index = 0; index < capacity; ++index) {
    // A slot no lattice point holds.
    if (probes[index].anchor[3] <= 0.f)
      continue;
    ++probeCount;
    const glm::vec3 ambient{probes[index].irradiance[0][0] * SH_DC_TO_IRRADIANCE, probes[index].irradiance[0][1] * SH_DC_TO_IRRADIANCE, probes[index].irradiance[0][2] * SH_DC_TO_IRRADIANCE};
    const auto magnitude = ambient.x + ambient.y + ambient.z;
    if (magnitude > 0.f)
      ++lit;
    brightest = std::max(brightest, magnitude);
    const glm::vec3 anchor{probes[index].anchor[0], probes[index].anchor[1], probes[index].anchor[2]};
    const glm::vec3 placed{probes[index].position[0], probes[index].position[1], probes[index].position[2]};
    const auto strayed = glm::length(placed - anchor);
    if (probes[index].position[3] < .5f)
      ++filled;
    if (probes[index].exterior > .5f)
      ++outside;
    if (strayed > probes[index].anchor[3] * .01f) {
      ++moved;
      displacement += strayed;
    }
    for (const auto texel : probes[index].depth) {
      const auto mean = UnpackHalf(texel & 0xFFFFu);
      reach += mean;
      deviation += UnpackHalf(texel >> 16);
      if (mean >= range * .999f)
        ++open;
    }
    auto standing = 0.;
    for (const auto word : probes[index].behind)
      for (auto slot = 0u; slot < 4u; ++slot)
        standing += static_cast<double>(word >> slot * 8 & 0xFFu) / 255.;
    behind += standing;
    // A tenth is a diagnostic's threshold and nothing shades through it: it separates a probe
    // that clipped a far side at one edge of its map from one standing squarely behind a surface,
    // and the second is the population this test exists for.
    if (standing > PROBE_DEPTH_TEXELS * .1)
      ++backfacing;
    // The outermost lattice plane, which the volume puts outside the scene it bounds -- the box is
    // padded by two percent of its own span -- and which therefore carries most of the trilinear
    // weight for every point on an outward facing wall. What these probes report and how much they
    // disagree among themselves is what a wall's bounce mostly is.
    const auto edge = glm::lessThan(glm::abs(anchor - origin), glm::vec3(1e-3f)) || glm::lessThan(glm::abs(anchor - (origin + glm::vec3(side))), glm::vec3(1e-3f));
    if (glm::any(edge)) {
      ++shell;
      shellMean += magnitude;
      shellSquare += static_cast<double>(magnitude) * magnitude;
    } else {
      ++coreCount;
      coreMean += magnitude;
      coreSquare += static_cast<double>(magnitude) * magnitude;
    }
    total += ambient;
    if (probes[index].position[0] < middle) {
      lower += ambient;
      ++lowerCount;
      continue;
    }
    upper += ambient;
    ++upperCount;
  }
  const CD3DX12_RANGE writeRange(0, 0);
  probeReadback->Unmap(0, &writeRange);
  probeReadback.Reset();
  if (probeCount == 0)
    return;
  total /= static_cast<float>(probeCount);
  if (lowerCount > 0)
    lower /= static_cast<float>(lowerCount);
  if (upperCount > 0)
    upper /= static_cast<float>(upperCount);
  spdlog::info("[DX12] Probe trace: {} of {} probes lit after {} frames, mean irradiance {:.4f} {:.4f} {:.4f}, brightest {:.3f}", lit, probeCount, tracedFrames, total.r, total.g, total.b, brightest);
  // What the visibility test has to work with. The reach is clamped at `PROBE_DEPTH_RANGE` of the
  // side, so a high open fraction is health rather than a fault: it says most directions hold nothing
  // within the only range reconstruction ever asks about, which is one leaf diagonal. A deviation of
  // nought everywhere would be the fault, since a variance is what separates a texel that saw one
  // flat surface from one that straddled an edge, and without it the test can only answer in steps.
  // A probe that moved is one the lattice had put inside geometry, where it would have reported
  // darkness for a room it was never in. What matters is not how many moved but that they no longer
  // have to be disbelieved for it.
  if (moved > 0)
    spdlog::info("[DX12] Probe trace: {} of {} probes walked out of the geometry they were placed in, by {:.3f} units on average", moved, probeCount, displacement / static_cast<double>(moved));
  else
    spdlog::info("[DX12] Probe trace: no probe needed to move, so the lattice landed none of them inside geometry");
  if (filled > 0)
    spdlog::info("[DX12] Probe trace: {} of {} probes are sealed in geometry and take their neighbours' estimate rather than their own", filled, probeCount);
  const auto texels = static_cast<double>(probeCount) * PROBE_DEPTH_TEXELS;
  spdlog::info("[DX12] Probe trace: visibility reaches {:.3f} units on average of a possible {:.3f}, spread {:.3f}, {:.1f}% of directions open to the clamp", reach / texels, range, deviation / texels, 100. * static_cast<double>(open) / texels);
  // What orientation adds to that. A scene whose walls are single quads puts probes on the far
  // side of them, and those probes measure the room through a surface they are behind; the share
  // is how much of the record says so, and it is the whole of what keeps them from being believed
  // through it. Nought everywhere means either a scene of closed solids or a trace that has
  // stopped recording which face it met, and the second is a fault the distances cannot show.
  spdlog::info("[DX12] Probe trace: {:.1f}% of the record was measured from the far side of a surface, and is refused on orientation rather than on distance", 100. * behind / texels);
  if (shell > 0 && coreCount > 0) {
    const auto shellAverage = shellMean / shell;
    const auto spread = std::sqrt(std::max(0., shellSquare / shell - shellAverage * shellAverage));
    // Both spreads, because only the pair says anything. The boundary shell carries most of the
    // trilinear weight for every point on an outward facing wall, so what those probes disagree
    // about is what a wall's bounce mostly is -- but a room is not uniform either, and the probes
    // inside it disagree by a room's worth. A shell spread far above the core's is the shell
    // holding something the room does not, which is the void outside it, and that is the grid. The
    // two landing together is the shell reporting the same room, at the same resolution.
    const auto coreAverage = coreMean / coreCount;
    const auto coreSpread = std::sqrt(std::max(0., coreSquare / coreCount - coreAverage * coreAverage));
    spdlog::info("[DX12] Probe trace: {} probes sit on the volume boundary, outside the scene, at mean irradiance {:.4f} with spread {:.4f}, against {:.4f} with spread {:.4f} for the {} inside it", shell, shellAverage, spread, coreAverage, coreSpread, coreCount);
  }
  if (backfacing > 0)
    spdlog::info("[DX12] Probe trace: {} of {} probes stand behind a surface for a tenth or more of their map, which distance alone would have believed through it", backfacing, probeCount);
  // How many probes the nearest surface they can see faces away from, against how many are sealed
  // in. The gap between the two is the population the buried test is blind to: probes outside the
  // scene with nothing near them, which only the reach of the trace's own rays can report on.
  if (outside > 0)
    spdlog::info("[DX12] Probe trace: {} of {} probes stand behind the scene's surfaces, and those take their neighbours' estimate whatever their own view", outside, probeCount);
  else
    spdlog::info("[DX12] Probe trace: every probe stands in front of the geometry, so each is judged on its own rays alone");
  if (lit == 0) {
    spdlog::error("[DX12] Probe trace: all {} probes are still black after {} frames", probeCount, tracedFrames);
    return;
  }
  const auto lowerSum = std::max(lower.r + lower.g + lower.b, 1e-6f);
  const auto upperSum = std::max(upper.r + upper.g + upper.b, 1e-6f);
  const auto lowerChroma = lower / lowerSum;
  const auto upperChroma = upper / upperSum;
  spdlog::info("[DX12] Probe trace: low x half {:.4f} {:.4f} {:.4f}, high x half {:.4f} {:.4f} {:.4f}", lower.r, lower.g, lower.b, upper.r, upper.g, upper.b);
  spdlog::info("[DX12] Probe trace: as chromaticity, low x {:.3f} {:.3f} {:.3f} against high x {:.3f} {:.3f} {:.3f}", lowerChroma.r, lowerChroma.g, lowerChroma.b, upperChroma.r, upperChroma.g, upperChroma.b);
  const auto separation = std::abs(lowerChroma.r - upperChroma.r) + std::abs(lowerChroma.g - upperChroma.g) + std::abs(lowerChroma.b - upperChroma.b);
  if (separation > CHROMA_SEPARATION) {
    spdlog::info("[DX12] Probe trace: the halves differ in chromaticity by {:.3f}, so the bounce carries where light came from and not merely how much", separation);
    return;
  }
  spdlog::warn("[DX12] Probe trace: the halves are the same colour to within {:.3f}, so the bounce may be carrying no surface colour at all", separation);
}
auto DXProbeVolume::GetNodeAddress() const -> D3D12_GPU_VIRTUAL_ADDRESS {
  return ready && nodeBuffer ? nodeBuffer->GetGPUVirtualAddress() : 0;
}
auto DXProbeVolume::GetProbeAddress() const -> D3D12_GPU_VIRTUAL_ADDRESS {
  return ready && probeBuffer ? probeBuffer->GetGPUVirtualAddress() : 0;
}
auto DXProbeVolume::GetLookupAddress() const -> D3D12_GPU_VIRTUAL_ADDRESS {
  return ready && lookupBuffer ? lookupBuffer->GetGPUVirtualAddress() : 0;
}
auto DXProbeVolume::GetBounds(float *outOrigin, float *outSide) const -> void {
  if (outOrigin)
    memcpy(outOrigin, glm::value_ptr(origin), sizeof(float) * 3);
  if (outSide)
    *outSide = side;
}
auto DXProbeVolume::GetProbeCount() const -> uint32_t {
  return capacity;
}
auto DXProbeVolume::GetNodeCount() const -> uint32_t {
  return PROBE_OCTREE_NODE_COUNT;
}
auto DXProbeVolume::GetLookupResolution() const -> uint32_t {
  return PROBE_LATTICE_CELLS;
}
auto DXProbeVolume::IsReady() const -> bool {
  return ready;
}
auto DXProbeVolume::Clear() -> void {
  if (owner)
    owner->WaitForGPU();
  nodeBuffer.Reset();
  probeBuffer.Reset();
  lookupBuffer.Reset();
  cornerBuffer.Reset();
  freeBuffer.Reset();
  counterBuffer.Reset();
  queueBuffer.Reset();
  boxBuffer.Reset();
  counterReadback.Reset();
  auditSeed.Reset();
  auditResult.Reset();
  auditReadback.Reset();
  probeReadback.Reset();
  boxData = nullptr;
  auditSeedData = nullptr;
  readbackGeneration.fill(0);
  probeState = D3D12_RESOURCE_STATE_COMMON;
  nodeState = D3D12_RESOURCE_STATE_COMMON;
  lookupState = D3D12_RESOURCE_STATE_COMMON;
  origin = {};
  side = 0.f;
  uniformDepth = 0;
  capacity = 0;
  grownFrom = 0;
  resetPending = false;
  placements.clear();
  meshBounds.clear();
  dirtyBoxes.clear();
  liveProbes = 0;
  leafCount = 0;
  deepestLeaf = 0;
  settled = false;
  loggedProbes = 0;
  loggedLeaves = 0;
  tracedFrames = 0;
  tracedSamples = 0;
  traceHash = 0;
  ready = false;
  validated = false;
  reported = false;
}
} // namespace kuki
#endif
