#include <dx_acceleration_structure.hpp>
#ifdef KUKI_HAS_DIRECTX
#include <algorithm>
#include <bit>
#include <cstring>
#include <dx_context.hpp>
#include <dx_pipeline.hpp>
#include <spdlog/spdlog.h>
#include <vector>
namespace kuki {
namespace {
constexpr uint32_t VALIDATION_SLOTS = 8;
constexpr uint32_t VALIDATION_GRID = 128;
constexpr uint32_t VALIDATION_GROUP = 8;
constexpr float VALIDATION_DISTANCE_SCALE = 1000.f;
auto CreateBuffer(ID3D12Device *device, const uint64_t bytes, const D3D12_HEAP_TYPE heapType, const D3D12_RESOURCE_STATES state, const D3D12_RESOURCE_FLAGS flags, const char *context) -> ComPtr<ID3D12Resource> {
  ComPtr<ID3D12Resource> buffer;
  if (!device || bytes == 0)
    return {};
  const auto properties = CD3DX12_HEAP_PROPERTIES(heapType);
  auto desc = CD3DX12_RESOURCE_DESC::Buffer(bytes, flags);
  if (DXFailed(device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&buffer)), context))
    return {};
  return buffer;
}
auto MapBuffer(ID3D12Resource *resource, const char *context) -> uint8_t * {
  void *mapped{};
  const CD3DX12_RANGE readRange(0, 0);
  if (!resource || DXFailed(resource->Map(0, &readRange, &mapped), context))
    return nullptr;
  return static_cast<uint8_t *>(mapped);
}
} // namespace
auto DXAccelerationStructure::DescribeGeometry(const DXMesh *mesh, D3D12_RAYTRACING_GEOMETRY_DESC &desc) const -> bool {
  if (!mesh || !mesh->vertexBuffer || mesh->vertexCount == 0)
    return false;
  desc = {};
  desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
  desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
  desc.Triangles.VertexBuffer.StartAddress = mesh->vertexBuffer->GetGPUVirtualAddress();
  desc.Triangles.VertexBuffer.StrideInBytes = mesh->vertexBufferView.StrideInBytes;
  desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
  desc.Triangles.VertexCount = mesh->vertexCount;
  if (mesh->indexCount > 0 && mesh->indexBuffer) {
    if (mesh->indexCount % 3 != 0)
      return false;
    desc.Triangles.IndexBuffer = mesh->indexBuffer->GetGPUVirtualAddress();
    desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    desc.Triangles.IndexCount = mesh->indexCount;
    return true;
  }
  return mesh->vertexCount % 3 == 0;
}
auto DXAccelerationStructure::CreateBufferView(ID3D12Resource *resource, const uint64_t bytes) -> uint32_t {
  if (!owner || !resource || bytes < 4)
    return DXDescriptorHeap::InvalidIndex;
  auto &heap = owner->GetSRVHeap();
  const auto index = heap.Allocate();
  if (index == DXDescriptorHeap::InvalidIndex)
    return index;
  D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
  desc.Format = DXGI_FORMAT_R32_TYPELESS;
  desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  desc.Buffer.NumElements = static_cast<UINT>(bytes / 4);
  desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
  owner->GetDevice()->CreateShaderResourceView(resource, &desc, heap.GetCPUHandle(index));
  return index;
}
auto DXAccelerationStructure::BuildMeshStructure(const DXMesh *mesh, const D3D12_GPU_VIRTUAL_ADDRESS scratchAddress) -> bool {
  D3D12_RAYTRACING_GEOMETRY_DESC geometry{};
  if (!owner || !DescribeGeometry(mesh, geometry))
    return false;
  auto *device = owner->GetDevice5();
  auto *commandList = owner->GetCommandList4();
  if (!device || !commandList)
    return false;
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
  inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
  inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
  inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  inputs.NumDescs = 1;
  inputs.pGeometryDescs = &geometry;
  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
  device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);
  if (info.ResultDataMaxSizeInBytes == 0)
    return false;
  auto &entry = meshStructures[mesh];
  entry.structure = CreateBuffer(device, info.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, "CreateCommittedResource for a bottom-level acceleration structure");
  if (!entry.structure)
    return false;
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
  build.Inputs = inputs;
  build.DestAccelerationStructureData = entry.structure->GetGPUVirtualAddress();
  build.ScratchAccelerationStructureData = scratchAddress;
  commandList->BuildRaytracingAccelerationStructure(&build, 0, nullptr);
  const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
  commandList->ResourceBarrier(1, &barrier);
  entry.vertexBuffer = CreateBufferView(mesh->vertexBuffer.Get(), mesh->vertexBufferView.SizeInBytes);
  if (mesh->indexCount > 0 && mesh->indexBuffer)
    entry.indexBuffer = CreateBufferView(mesh->indexBuffer.Get(), mesh->indexBufferView.SizeInBytes);
  meshStructureBytes += info.ResultDataMaxSizeInBytes;
  return true;
}
auto DXAccelerationStructure::EnsureCapacity(const uint32_t count) -> bool {
  if (count == 0 || !owner)
    return false;
  if (count <= capacity && instanceDescData && geometryInfoData)
    return true;
  auto *device = owner->GetDevice();
  const auto grown = count + count / 2 + 16;
  owner->WaitForGPU();
  instanceDescs.Reset();
  geometryInfo.Reset();
  instanceDescData = nullptr;
  geometryInfoData = nullptr;
  capacity = 0;
  const auto instanceBytes = static_cast<uint64_t>(grown) * sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * DX_FRAME_COUNT;
  const auto recordBytes = static_cast<uint64_t>(grown) * sizeof(DXGeometryInfo) * DX_FRAME_COUNT;
  instanceDescs = CreateBuffer(device, instanceBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE, "CreateCommittedResource for raytracing instance descriptions");
  geometryInfo = CreateBuffer(device, recordBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE, "CreateCommittedResource for raytracing geometry records");
  instanceDescData = MapBuffer(instanceDescs.Get(), "Map raytracing instance descriptions");
  geometryInfoData = MapBuffer(geometryInfo.Get(), "Map raytracing geometry records");
  if (!instanceDescData || !geometryInfoData) {
    instanceDescs.Reset();
    geometryInfo.Reset();
    instanceDescData = nullptr;
    geometryInfoData = nullptr;
    return false;
  }
  capacity = grown;
  topLevel.Reset();
  topLevelBytes = 0;
  return true;
}
auto DXAccelerationStructure::EnsureScratch(const uint64_t bytes) -> bool {
  if (!owner || bytes == 0)
    return false;
  if (scratch && scratchBytes >= bytes)
    return true;
  owner->WaitForGPU();
  scratch.Reset();
  scratchBytes = 0;
  scratch = CreateBuffer(owner->GetDevice(), bytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, "CreateCommittedResource for acceleration structure scratch");
  if (!scratch)
    return false;
  scratchBytes = bytes;
  return true;
}
auto DXAccelerationStructure::EnsureTopLevel(const uint64_t bytes) -> bool {
  if (!owner || bytes == 0)
    return false;
  if (topLevel && topLevelBytes >= bytes)
    return true;
  owner->WaitForGPU();
  topLevel.Reset();
  topLevelBytes = 0;
  topLevel = CreateBuffer(owner->GetDevice(), bytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, "CreateCommittedResource for the top-level acceleration structure");
  if (!topLevel)
    return false;
  topLevelBytes = bytes;
  return true;
}
auto DXAccelerationStructure::Build(DXContext &context, const std::vector<DXRayInstance> &instances) -> bool {
  ready = false;
  instanceCount = 0;
  owner = &context;
  auto *device = context.GetDevice5();
  auto *commandList = context.GetCommandList4();
  if (!device || !commandList || !context.GetCapabilities().SupportsInlineRaytracing())
    return false;
  std::vector<const DXRayInstance *> placed;
  std::vector<const DXMesh *> pending;
  uint64_t requiredScratch = 0;
  placed.reserve(instances.size());
  for (const auto &instance : instances) {
    if (!instance.mesh || !*instance.mesh || instance.mesh->skinned)
      continue;
    D3D12_RAYTRACING_GEOMETRY_DESC geometry{};
    if (!DescribeGeometry(instance.mesh, geometry))
      continue;
    placed.push_back(&instance);
    if (meshStructures.contains(instance.mesh))
      continue;
    meshStructures[instance.mesh] = {};
    pending.push_back(instance.mesh);
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    inputs.NumDescs = 1;
    inputs.pGeometryDescs = &geometry;
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);
    requiredScratch = std::max(requiredScratch, info.ScratchDataSizeInBytes);
  }
  if (placed.empty())
    return false;
  if (!EnsureCapacity(static_cast<uint32_t>(placed.size())))
    return false;
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topInputs{};
  topInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
  topInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
  topInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  topInputs.NumDescs = capacity;
  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topInfo{};
  device->GetRaytracingAccelerationStructurePrebuildInfo(&topInputs, &topInfo);
  requiredScratch = std::max(requiredScratch, topInfo.ScratchDataSizeInBytes);
  if (!EnsureScratch(requiredScratch) || !EnsureTopLevel(topInfo.ResultDataMaxSizeInBytes))
    return false;
  for (const auto *mesh : pending)
    if (!BuildMeshStructure(mesh, scratch->GetGPUVirtualAddress()))
      spdlog::warn("[DX12] could not build a bottom-level acceleration structure; its geometry will not be traced.");
  const auto frame = context.GetFrameIndex() % DX_FRAME_COUNT;
  const auto instanceOffset = static_cast<uint64_t>(frame) * capacity * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
  const auto recordOffset = static_cast<uint64_t>(frame) * capacity * sizeof(DXGeometryInfo);
  auto *descs = reinterpret_cast<D3D12_RAYTRACING_INSTANCE_DESC *>(instanceDescData + instanceOffset);
  auto *records = reinterpret_cast<DXGeometryInfo *>(geometryInfoData + recordOffset);
  uint32_t written = 0;
  for (const auto *instance : placed) {
    const auto it = meshStructures.find(instance->mesh);
    if (it == meshStructures.end() || !it->second.structure)
      continue;
    auto &desc = descs[written];
    desc = {};
    for (auto row = 0; row < 3; ++row)
      for (auto column = 0; column < 4; ++column)
        desc.Transform[row][column] = instance->transform[column * 4 + row];
    const auto opaque = instance->alphaMode == 0 && instance->transmission <= .0f;
    desc.InstanceID = written;
    desc.InstanceMask = 0xFF;
    desc.Flags = opaque ? D3D12_RAYTRACING_INSTANCE_FLAG_NONE : D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE;
    desc.AccelerationStructure = it->second.structure->GetGPUVirtualAddress();
    auto &record = records[written];
    record = {};
    record.vertexBuffer = it->second.vertexBuffer;
    record.indexBuffer = it->second.indexBuffer;
    record.materialTextures = instance->materialTextures;
    record.textureMask = instance->textureMask;
    memcpy(record.albedo, instance->albedo, sizeof(record.albedo));
    memcpy(record.emissive, instance->emissive, sizeof(record.emissive));
    memcpy(record.attenuation, instance->attenuation, sizeof(record.attenuation));
    record.transmission = instance->transmission;
    record.thickness = instance->thickness;
    record.alphaCutoff = instance->alphaCutoff;
    record.alphaMode = instance->alphaMode;
    record.entityId = instance->entityId;
    ++written;
  }
  if (written == 0)
    return false;
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
  build.Inputs = topInputs;
  build.Inputs.NumDescs = written;
  build.Inputs.InstanceDescs = instanceDescs->GetGPUVirtualAddress() + instanceOffset;
  build.DestAccelerationStructureData = topLevel->GetGPUVirtualAddress();
  build.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
  commandList->BuildRaytracingAccelerationStructure(&build, 0, nullptr);
  const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
  commandList->ResourceBarrier(1, &barrier);
  geometryInfoAddress = geometryInfo->GetGPUVirtualAddress() + recordOffset;
  instanceCount = written;
  ready = true;
  return true;
}
auto DXAccelerationStructure::EnsureValidationBuffers() -> bool {
  if (validationSeedData && validationResult && validationReadback)
    return true;
  auto *device = owner ? owner->GetDevice() : nullptr;
  constexpr uint64_t BYTES = VALIDATION_SLOTS * sizeof(uint32_t);
  validationSeed = CreateBuffer(device, BYTES, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE, "CreateCommittedResource for the raytracing validation seed");
  validationResult = CreateBuffer(device, BYTES, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, "CreateCommittedResource for the raytracing validation result");
  validationReadback = CreateBuffer(device, BYTES, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE, "CreateCommittedResource for the raytracing validation readback");
  validationSeedData = MapBuffer(validationSeed.Get(), "Map the raytracing validation seed");
  if (validationSeedData && validationResult && validationReadback)
    return true;
  validationSeed.Reset();
  validationResult.Reset();
  validationReadback.Reset();
  validationSeedData = nullptr;
  return false;
}
auto DXAccelerationStructure::Validate(DXContext &context, DXPipelineCache &pipelines, const float *inverseViewProjection, const float *position) -> void {
  if (validated || !ready || !inverseViewProjection || !position)
    return;
  validated = true;
  auto *commandList = context.GetCommandList();
  const auto pipeline = pipelines.GetRayProbePipeline(context.GetDevice());
  if (!commandList || !pipeline || !*pipeline || !EnsureValidationBuffers())
    return;
  uint32_t seed[VALIDATION_SLOTS]{};
  seed[2] = 0xFFFFFFFFu;
  memcpy(validationSeedData, seed, sizeof(seed));
  commandList->CopyBufferRegion(validationResult.Get(), 0, validationSeed.Get(), 0, sizeof(seed));
  auto toWrite = CD3DX12_RESOURCE_BARRIER::Transition(validationResult.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  commandList->ResourceBarrier(1, &toWrite);
  DXRayProbeConstants constants{};
  memcpy(constants.inverseViewProjection, inverseViewProjection, sizeof(constants.inverseViewProjection));
  memcpy(constants.origin, position, sizeof(float) * 3);
  constants.dimensions[0] = VALIDATION_GRID;
  constants.dimensions[1] = VALIDATION_GRID;
  commandList->SetComputeRootSignature(pipeline->rootSignature.Get());
  commandList->SetPipelineState(pipeline->pipelineState.Get());
  commandList->SetComputeRoot32BitConstants(0, sizeof(DXRayProbeConstants) / sizeof(uint32_t), &constants, 0);
  commandList->SetComputeRootShaderResourceView(1, topLevel->GetGPUVirtualAddress());
  commandList->SetComputeRootShaderResourceView(2, geometryInfoAddress);
  commandList->SetComputeRootUnorderedAccessView(3, validationResult->GetGPUVirtualAddress());
  commandList->Dispatch(VALIDATION_GRID / VALIDATION_GROUP, VALIDATION_GRID / VALIDATION_GROUP, 1);
  auto toRead = CD3DX12_RESOURCE_BARRIER::Transition(validationResult.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
  commandList->ResourceBarrier(1, &toRead);
  commandList->CopyBufferRegion(validationReadback.Get(), 0, validationResult.Get(), 0, sizeof(seed));
  auto toSeed = CD3DX12_RESOURCE_BARRIER::Transition(validationResult.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
  commandList->ResourceBarrier(1, &toSeed);
  context.FlushCommandList();
  uint32_t *mapped{};
  const CD3DX12_RANGE readRange(0, sizeof(seed));
  if (DXFailed(validationReadback->Map(0, &readRange, reinterpret_cast<void **>(&mapped)), "Map the raytracing validation readback"))
    return;
  uint32_t result[VALIDATION_SLOTS]{};
  memcpy(result, mapped, sizeof(result));
  const CD3DX12_RANGE writeRange(0, 0);
  validationReadback->Unmap(0, &writeRange);
  const auto traced = VALIDATION_GRID * VALIDATION_GRID;
  spdlog::info("[DX12] ray scene: {} instances over {} bottom-level structures, {:.2f} MB", instanceCount, meshStructures.size(), static_cast<double>(meshStructureBytes + topLevelBytes) / (1024. * 1024.));
  if (result[0] == 0) {
    spdlog::warn("[DX12] ray scene: none of {} camera rays hit anything. The structures traverse but hold no geometry where the camera is looking.", traced);
    return;
  }
  spdlog::info("[DX12] ray scene: {} of {} camera rays hit, at depths {:.3f} to {:.3f}", result[0], traced, static_cast<float>(result[2]) / VALIDATION_DISTANCE_SCALE, static_cast<float>(result[3]) / VALIDATION_DISTANCE_SCALE);
  spdlog::info("[DX12] ray scene: {} hits resolved a vertex buffer and {} a material table, over at least {} distinct entities", result[5], result[6], std::popcount(result[4]));
  if (result[5] < result[0] || result[6] < result[0])
    spdlog::warn("[DX12] ray scene: some hits resolved to no geometry or no material, so their records were never filled in.");
}
auto DXAccelerationStructure::GetTopLevelAddress() const -> D3D12_GPU_VIRTUAL_ADDRESS {
  return ready && topLevel ? topLevel->GetGPUVirtualAddress() : 0;
}
auto DXAccelerationStructure::GetGeometryInfoAddress() const -> D3D12_GPU_VIRTUAL_ADDRESS {
  return ready ? geometryInfoAddress : 0;
}
auto DXAccelerationStructure::GetInstanceCount() const -> uint32_t {
  return instanceCount;
}
auto DXAccelerationStructure::IsReady() const -> bool {
  return ready;
}
auto DXAccelerationStructure::Clear() -> void {
  if (owner) {
    owner->WaitForGPU();
    auto &heap = owner->GetSRVHeap();
    for (const auto &[mesh, entry] : meshStructures) {
      heap.Free(entry.vertexBuffer);
      heap.Free(entry.indexBuffer);
    }
  }
  meshStructures.clear();
  topLevel.Reset();
  scratch.Reset();
  instanceDescs.Reset();
  geometryInfo.Reset();
  validationSeed.Reset();
  validationResult.Reset();
  validationReadback.Reset();
  instanceDescData = nullptr;
  geometryInfoData = nullptr;
  validationSeedData = nullptr;
  geometryInfoAddress = 0;
  scratchBytes = 0;
  topLevelBytes = 0;
  meshStructureBytes = 0;
  capacity = 0;
  instanceCount = 0;
  ready = false;
  validated = false;
}
} // namespace kuki
#endif
