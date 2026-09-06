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
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
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
constexpr uint32_t INVALID_NODE = 0xFFFFFFFF;
constexpr uint32_t REPORT_AFTER_FRAMES = 90;
constexpr float SH_DC_TO_IRRADIANCE = .886227f;
constexpr float CHROMA_SEPARATION = .02f;
/// @brief The six cells sharing a face with one cell, which is how a fill and a vote both step.
constexpr glm::ivec3 CELL_FACES[6]{{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
auto RandomRotation(std::mt19937 &engine) -> glm::mat3 {
  std::uniform_real_distribution<float> distribution(0.f, 1.f);
  const auto u1 = distribution(engine);
  const auto u2 = distribution(engine) * glm::two_pi<float>();
  const auto u3 = distribution(engine) * glm::two_pi<float>();
  const auto a = std::sqrt(1.f - u1);
  const auto b = std::sqrt(u1);
  return glm::mat3_cast(glm::quat(b * std::cos(u3), a * std::sin(u2), a * std::cos(u2), b * std::sin(u3)));
}
struct BuildNode {
  glm::vec3 center{};
  float extent{};
  uint32_t depth{};
  uint32_t children[8]{};
  bool leaf{};
  /// @brief Clusters overlapping this cell, by index. Dropped once the node becomes a leaf.
  std::vector<uint32_t> clusters;
  /// @brief Triangles this cell holds, estimated from how much of each cluster falls inside it.
  ///
  /// Fractional because a cluster is a box: a cell covering half of one is credited with half its
  /// triangles. The leaf threshold compares against this, so it keeps meaning what it meant when
  /// every triangle was tested individually.
  float density{};
};
/// @brief The half-precision bits of a finite, non-negative float, which is all a probe stores.
///
/// Only the cases a distance can take: no infinities, no negatives, no subnormals. Anything too
/// small to be a half becomes nought, which is what a deviation of nothing means anyway, and
/// anything too large saturates rather than turning into an infinity the shader would spread.
constexpr auto PackHalf(const float value) -> uint32_t {
  if (!(value > 0.f))
    return 0;
  const auto bits = std::bit_cast<uint32_t>(value);
  const auto exponent = static_cast<int32_t>((bits >> 23) & 0xFFu) - 127 + 15;
  if (exponent <= 0)
    return 0;
  if (exponent >= 31)
    return 0x7BFFu;
  return (static_cast<uint32_t>(exponent) << 10) | ((bits >> 13) & 0x3FFu);
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
auto Combine(size_t &hash, const size_t value) -> void {
  hash ^= value + 0x9E3779B97F4A7C15ull + (hash << 6) + (hash >> 2);
}
auto HashGeometry(const std::vector<DXProbeGeometry> &geometry) -> size_t {
  size_t hash = geometry.size();
  for (const auto &item : geometry) {
    Combine(hash, reinterpret_cast<uintptr_t>(item.vertices.data()));
    Combine(hash, item.vertices.size());
    Combine(hash, item.indices.size());
    const auto *values = glm::value_ptr(item.transform);
    for (auto i = 0; i < 16; ++i)
      Combine(hash, std::bit_cast<uint32_t>(values[i]));
  }
  return hash;
}
/// @brief Spreads the low ten bits of a value out into every third bit.
auto ExpandBits(uint32_t value) -> uint32_t {
  value = (value * 0x00010001u) & 0xFF0000FFu;
  value = (value * 0x00000101u) & 0x0F00F00Fu;
  value = (value * 0x00000011u) & 0xC30C30C3u;
  value = (value * 0x00000005u) & 0x49249249u;
  return value;
}
/// @brief Morton code of a point given in unit coordinates, ten bits an axis.
///
/// Interleaving the axes makes numeric order approximate spatial order, so sorting on this puts
/// triangles that sit near each other next to each other in the array.
auto Morton(const glm::vec3 &unit) -> uint32_t {
  const auto x = static_cast<uint32_t>(std::clamp(unit.x * 1024.f, 0.f, 1023.f));
  const auto y = static_cast<uint32_t>(std::clamp(unit.y * 1024.f, 0.f, 1023.f));
  const auto z = static_cast<uint32_t>(std::clamp(unit.z * 1024.f, 0.f, 1023.f));
  return (ExpandBits(x) << 2) | (ExpandBits(y) << 1) | ExpandBits(z);
}
/// @brief One triangle reduced to its box and its place along a space-filling curve.
struct ClusterTriangle {
  glm::vec3 low{};
  glm::vec3 high{};
  uint32_t code{};
};
/// @brief Groups a mesh's triangles into spatially compact runs and reduces each run to a box.
///
/// The grouping is by Morton order, not index order, and that is the whole of what makes this
/// worth doing. Chunking triangles as the index buffer happens to list them produces boxes that
/// span the mesh: measured on a real model, a run of 32 consecutive triangles had a box 31 times
/// the diagonal of the triangles in it, some 30000 times the volume. Subdivision would then push
/// every one of those boxes into nearly every cell it tested, and sorting 32 times fewer items
/// against far more cells each is slower than sorting the triangles was.
///
/// Sorting first costs one pass per mesh and is cached with the clusters, so a rebuild never pays
/// it. What a rebuild pays is eight corners through a matrix per cluster.
///
/// Local space, so the result survives the mesh being moved.
auto BuildClusters(const DXProbeGeometry &geometry) -> std::vector<DXProbeCluster> {
  std::vector<ClusterTriangle> triangles;
  auto meshLow = glm::vec3(std::numeric_limits<float>::max());
  auto meshHigh = glm::vec3(std::numeric_limits<float>::lowest());
  const auto Add = [&](const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c) {
    ClusterTriangle triangle;
    triangle.low = glm::min(a, glm::min(b, c));
    triangle.high = glm::max(a, glm::max(b, c));
    meshLow = glm::min(meshLow, triangle.low);
    meshHigh = glm::max(meshHigh, triangle.high);
    triangles.push_back(triangle);
  };
  if (!geometry.indices.empty()) {
    triangles.reserve(geometry.indices.size() / 3);
    for (size_t i = 0; i + 2 < geometry.indices.size(); i += 3) {
      const auto x = geometry.indices[i];
      const auto y = geometry.indices[i + 1];
      const auto z = geometry.indices[i + 2];
      if (x < geometry.vertices.size() && y < geometry.vertices.size() && z < geometry.vertices.size())
        Add(geometry.vertices[x].position, geometry.vertices[y].position, geometry.vertices[z].position);
    }
  } else {
    triangles.reserve(geometry.vertices.size() / 3);
    for (size_t i = 0; i + 2 < geometry.vertices.size(); i += 3)
      Add(geometry.vertices[i].position, geometry.vertices[i + 1].position, geometry.vertices[i + 2].position);
  }
  if (triangles.empty())
    return {};
  const auto span = meshHigh - meshLow;
  const glm::vec3 inverse{span.x > 1e-6f ? 1.f / span.x : 0.f, span.y > 1e-6f ? 1.f / span.y : 0.f, span.z > 1e-6f ? 1.f / span.z : 0.f};
  for (auto &triangle : triangles)
    triangle.code = Morton(((triangle.low + triangle.high) * .5f - meshLow) * inverse);
  std::sort(triangles.begin(), triangles.end(), [](const ClusterTriangle &a, const ClusterTriangle &b) { return a.code < b.code; });
  std::vector<DXProbeCluster> clusters;
  clusters.reserve(triangles.size() / PROBE_CLUSTER_TRIANGLES + 1);
  for (size_t first = 0; first < triangles.size(); first += PROBE_CLUSTER_TRIANGLES) {
    const auto count = std::min<size_t>(PROBE_CLUSTER_TRIANGLES, triangles.size() - first);
    DXProbeCluster cluster{.low = triangles[first].low, .high = triangles[first].high, .triangles = static_cast<uint32_t>(count)};
    for (size_t offset = 1; offset < count; ++offset) {
      cluster.low = glm::min(cluster.low, triangles[first + offset].low);
      cluster.high = glm::max(cluster.high, triangles[first + offset].high);
    }
    clusters.push_back(cluster);
  }
  return clusters;
}
/// @brief Puts a local-space cluster where its mesh is, as a box that still contains it.
///
/// Eight corners through the matrix and a fresh box around the result. Conservative under rotation,
/// which is the price of staying axis-aligned and is why this is cheap enough to redo per build.
auto TransformCluster(const DXProbeCluster &cluster, const glm::mat4 &transform) -> DXProbeCluster {
  auto low = glm::vec3(std::numeric_limits<float>::max());
  auto high = glm::vec3(std::numeric_limits<float>::lowest());
  for (uint32_t corner = 0; corner < 8; ++corner) {
    const glm::vec3 local{(corner & 1) ? cluster.high.x : cluster.low.x, (corner & 2) ? cluster.high.y : cluster.low.y, (corner & 4) ? cluster.high.z : cluster.low.z};
    const auto world = glm::vec3(transform * glm::vec4(local, 1.f));
    low = glm::min(low, world);
    high = glm::max(high, world);
  }
  return {.low = low, .high = high, .triangles = cluster.triangles};
}
auto OverlapsCell(const DXProbeCluster &cluster, const glm::vec3 &center, const float extent) -> bool {
  return glm::all(glm::lessThanEqual(cluster.low, center + extent)) && glm::all(glm::greaterThanEqual(cluster.high, center - extent));
}
/// @brief Share of one axis of a cluster that a cell covers. A flat cluster counts as fully covered.
auto AxisFraction(const float overlap, const float span) -> float {
  return span > 1e-6f ? std::min(overlap / span, 1.f) : 1.f;
}
/// @brief How much of a cluster lies inside a cell, as a fraction of the cluster.
///
/// Without this a cluster would contribute its whole triangle count to all eight children it
/// straddles, so density would grow with every level and every node would look dense enough to
/// split. Weighting by the shared volume keeps a subdivided node's children summing to roughly what
/// the parent held, which is what the leaf threshold was tuned against when it counted triangles.
auto OverlapFraction(const DXProbeCluster &cluster, const glm::vec3 &center, const float extent) -> float {
  const auto low = glm::max(cluster.low, center - extent);
  const auto high = glm::min(cluster.high, center + extent);
  const auto overlap = high - low;
  if (overlap.x < 0.f || overlap.y < 0.f || overlap.z < 0.f)
    return 0.f;
  const auto span = cluster.high - cluster.low;
  return AxisFraction(overlap.x, span.x) * AxisFraction(overlap.y, span.y) * AxisFraction(overlap.z, span.z);
}
auto CornerOffset(const uint32_t corner, const float extent) -> glm::vec3 {
  return {(corner & 1) ? extent : -extent, (corner & 2) ? extent : -extent, (corner & 4) ? extent : -extent};
}
auto Subdivide(std::vector<BuildNode> &nodes, const uint32_t index, const std::vector<DXProbeCluster> &clusters) -> void {
  const auto depth = nodes[index].depth;
  const auto dense = nodes[index].density > static_cast<float>(PROBE_OCTREE_LEAF_TRIANGLES);
  if (depth >= PROBE_OCTREE_MAX_DEPTH || (depth >= PROBE_OCTREE_MIN_DEPTH && !dense)) {
    nodes[index].leaf = true;
    nodes[index].clusters.clear();
    nodes[index].clusters.shrink_to_fit();
    return;
  }
  const auto center = nodes[index].center;
  const auto childExtent = nodes[index].extent * .5f;
  const auto inherited = std::move(nodes[index].clusters);
  nodes[index].clusters.clear();
  nodes[index].clusters.shrink_to_fit();
  for (uint32_t corner = 0; corner < 8; ++corner) {
    BuildNode child;
    child.center = center + CornerOffset(corner, childExtent);
    child.extent = childExtent;
    child.depth = depth + 1;
    for (const auto cluster : inherited)
      if (OverlapsCell(clusters[cluster], child.center, childExtent)) {
        child.clusters.push_back(cluster);
        child.density += static_cast<float>(clusters[cluster].triangles) * OverlapFraction(clusters[cluster], child.center, childExtent);
      }
    nodes[index].children[corner] = static_cast<uint32_t>(nodes.size());
    nodes.push_back(std::move(child));
  }
  for (uint32_t corner = 0; corner < 8; ++corner)
    Subdivide(nodes, nodes[index].children[corner], clusters);
}
/// @brief Which side of the scene's surfaces one cell of the classification grid stands on.
///
/// `Solid` is a cell some triangle passes through. `Facing` and `Behind` are free cells the geometry
/// faces towards and away from. `Unknown` is free space no surface has reached, which the fill
/// resolves and which survives only in a volume holding no geometry at all.
enum class CellSide : uint8_t {
  Unknown,
  Solid,
  Facing,
  Behind
};
/// @brief The geometry inside one solid cell, reduced to a single oriented plane.
///
/// Area weighted, so a cell that a wall passes through and a stray decal clips reads as the wall.
/// The point is the mean centroid, which lies on the surface where the cell holds one flat piece of
/// it and near it otherwise; with the normal it is what lets a cell beside this one ask which side
/// of the surface it is on.
struct CellSurface {
  glm::vec3 normal{};
  glm::vec3 point{};
  float area{};
};
/// @brief What each cell of the volume holds, and which side of the geometry it stands on.
struct CellField {
  std::vector<CellSurface> surface;
  std::vector<uint8_t> side;
};
/// @brief Sorts the volume's free space into the side of the geometry each part of it stands on.
///
/// Answers the one question `trust` had no way to ask. Relocation moves a probe out of geometry and
/// the buried test disbelieves one sealed inside it, and both read the probe's own rays -- so both
/// are blind to the probe that is merely somewhere irrelevant: outside a wall, past the edge of
/// every surface, with a clear view of nothing but sky. That probe has an open view and is trusted
/// with it, and a point on the inner face of the wall beside it takes its estimate at a trilinear
/// share. A share that repeats with the lattice is the grid.
///
/// Connectivity alone cannot separate the two sides, and a Cornell box shows why: five walls and an
/// open front, so the room and the void outside it are one connected region of free space and a
/// fill seeded from where the volume ends swallows both. Orientation can separate them, and
/// exactly: a surface faces one way, the space on that side is the space it lights, and the space
/// behind it is not. So the fill is seeded from what the geometry says rather than from where the
/// volume ends -- every free cell beside a solid one takes the side its surface faces -- and only
/// then spreads through free space to the cells no surface reached. A cell out in a void inherits
/// from the nearest cell that was labelled, which is the nearest surface's verdict about it.
///
/// The grid is the lookup grid's own resolution, so probe anchors land on cell corners rather than
/// somewhere inside a cell, and a probe reads the cells that touch it instead of one it was rounded
/// into.
auto ClassifySpace(const std::vector<DXProbeGeometry> &geometry, const glm::vec3 &origin, const float side, const uint32_t lattice) -> CellField {
  CellField field;
  const auto width = static_cast<size_t>(lattice);
  const auto cells = width * width * width;
  field.surface.resize(cells);
  field.side.assign(cells, static_cast<uint8_t>(CellSide::Unknown));
  const auto cellSize = side / static_cast<float>(lattice);
  const auto last = static_cast<int>(lattice) - 1;
  const auto Index = [width](const int x, const int y, const int z) { return static_cast<size_t>(x) + width * (static_cast<size_t>(y) + width * static_cast<size_t>(z)); };
  const auto Cell = [&](const glm::vec3 &point) {
    const auto local = (point - origin) / cellSize;
    return glm::ivec3{std::clamp(static_cast<int>(std::floor(local.x)), 0, last), std::clamp(static_cast<int>(std::floor(local.y)), 0, last), std::clamp(static_cast<int>(std::floor(local.z)), 0, last)};
  };
  const auto Center = [&](const glm::ivec3 &at) { return origin + (glm::vec3{static_cast<float>(at.x), static_cast<float>(at.y), static_cast<float>(at.z)} + .5f) * cellSize; };
  const auto Beyond = [last](const glm::ivec3 &at) { return glm::any(glm::lessThan(at, glm::ivec3(0))) || glm::any(glm::greaterThan(at, glm::ivec3(last))); };
  {
    KUKI_PROFILE_SCOPE("ClassifySurfaces");
    for (const auto &item : geometry) {
      if (item.vertices.empty())
        continue;
      const auto rotation = glm::inverseTranspose(glm::mat3(item.transform));
      const auto Add = [&](const size_t first, const size_t second, const size_t third) {
        const auto a = glm::vec3(item.transform * glm::vec4(item.vertices[first].position, 1.f));
        const auto b = glm::vec3(item.transform * glm::vec4(item.vertices[second].position, 1.f));
        const auto c = glm::vec3(item.transform * glm::vec4(item.vertices[third].position, 1.f));
        const auto cross = glm::cross(b - a, c - a);
        const auto area = glm::length(cross);
        if (area < 1e-12f)
          return;
        // The vertex normals rather than the winding, because the trace decides which face a ray met
        // from the interpolated vertex normal, and the two disagreeing would put this and the buried
        // test on opposite sides of the same wall. The winding is the fallback for geometry that
        // arrived without normals, where it is the only orientation there is.
        auto normal = rotation * (item.vertices[first].normal + item.vertices[second].normal + item.vertices[third].normal);
        normal = glm::dot(normal, normal) > 1e-12f ? glm::normalize(normal) : cross / area;
        const auto centroid = (a + b + c) / 3.f;
        const auto low = Cell(glm::min(a, glm::min(b, c)));
        const auto high = Cell(glm::max(a, glm::max(b, c)));
        // The triangle's box rather than the triangle, so a diagonal one claims cells it only passes
        // near. Claiming too much leaves free space labelled from a surface slightly further off and
        // never from the wrong side of a wall, so the error this makes is the safe one.
        for (auto z = low.z; z <= high.z; ++z)
          for (auto y = low.y; y <= high.y; ++y)
            for (auto x = low.x; x <= high.x; ++x) {
              const auto at = Index(x, y, z);
              field.side[at] = static_cast<uint8_t>(CellSide::Solid);
              field.surface[at].normal += normal * area;
              field.surface[at].point += centroid * area;
              field.surface[at].area += area;
            }
      };
      if (!item.indices.empty()) {
        for (size_t i = 0; i + 2 < item.indices.size(); i += 3) {
          const auto x = item.indices[i];
          const auto y = item.indices[i + 1];
          const auto z = item.indices[i + 2];
          if (x < item.vertices.size() && y < item.vertices.size() && z < item.vertices.size())
            Add(x, y, z);
        }
        continue;
      }
      for (size_t i = 0; i + 2 < item.vertices.size(); i += 3)
        Add(i, i + 1, i + 2);
    }
  }
  std::vector<float> vote(cells, 0.f);
  {
    KUKI_PROFILE_SCOPE("ClassifySeed");
    for (auto z = 0; z <= last; ++z)
      for (auto y = 0; y <= last; ++y)
        for (auto x = 0; x <= last; ++x) {
          const auto at = Index(x, y, z);
          if (field.side[at] != static_cast<uint8_t>(CellSide::Solid))
            continue;
          const auto &held = field.surface[at];
          if (held.area <= 0.f || glm::dot(held.normal, held.normal) <= 0.f)
            continue;
          const auto normal = glm::normalize(held.normal);
          const auto point = held.point / held.area;
          for (const auto &face : CELL_FACES) {
            const auto to = glm::ivec3{x, y, z} + face;
            if (Beyond(to))
              continue;
            const auto neighbour = Index(to.x, to.y, to.z);
            if (field.side[neighbour] == static_cast<uint8_t>(CellSide::Solid))
              continue;
            // The sign of the side, weighted by how much surface is voting, so a wall outvotes a
            // speck sharing its cell. How far the cell is carries nothing the sign does not, since
            // by construction it is one cell either way.
            vote[neighbour] += glm::dot(normal, Center(to) - point) < 0.f ? -held.area : held.area;
          }
        }
  }
  std::vector<size_t> queue;
  queue.reserve(cells);
  for (size_t at = 0; at < cells; ++at) {
    if (vote[at] == 0.f)
      continue;
    field.side[at] = static_cast<uint8_t>(vote[at] < 0.f ? CellSide::Behind : CellSide::Facing);
    queue.push_back(at);
  }
  {
    // Breadth first from every labelled cell at once, so a cell no surface reached takes the verdict
    // of the nearest one that did rather than of whichever the sweep happened to visit first. Solid
    // cells are never entered, which is what keeps a verdict from crossing the wall that made it.
    KUKI_PROFILE_SCOPE("ClassifyFill");
    for (size_t head = 0; head < queue.size(); ++head) {
      const auto label = field.side[queue[head]];
      const glm::ivec3 home{static_cast<int>(queue[head] % width), static_cast<int>(queue[head] / width % width), static_cast<int>(queue[head] / (width * width))};
      for (const auto &face : CELL_FACES) {
        const auto to = home + face;
        if (Beyond(to))
          continue;
        const auto neighbour = Index(to.x, to.y, to.z);
        if (field.side[neighbour] != static_cast<uint8_t>(CellSide::Unknown))
          continue;
        field.side[neighbour] = label;
        queue.push_back(neighbour);
      }
    }
  }
  return field;
}
auto DescendToLeaf(const std::vector<BuildNode> &nodes, const glm::vec3 &point) -> uint32_t {
  uint32_t index = 0;
  while (!nodes[index].leaf) {
    uint32_t corner = 0;
    if (point.x >= nodes[index].center.x)
      corner |= 1;
    if (point.y >= nodes[index].center.y)
      corner |= 2;
    if (point.z >= nodes[index].center.z)
      corner |= 4;
    index = nodes[index].children[corner];
  }
  return index;
}
} // namespace
auto DXProbeVolume::UploadBuffer(const void *data, const uint64_t bytes, const D3D12_RESOURCE_FLAGS flags, const char *context) -> ComPtr<ID3D12Resource> {
  auto *device = owner ? owner->GetDevice() : nullptr;
  auto *commandList = owner ? owner->GetCommandList() : nullptr;
  if (!device || !commandList || !data || bytes == 0)
    return {};
  ComPtr<ID3D12Resource> source;
  ComPtr<ID3D12Resource> destination;
  const auto uploadProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  const auto defaultProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(bytes);
  auto defaultDesc = CD3DX12_RESOURCE_DESC::Buffer(bytes, flags);
  if (DXFailed(device->CreateCommittedResource(&uploadProperties, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&source)), context))
    return {};
  if (DXFailed(device->CreateCommittedResource(&defaultProperties, D3D12_HEAP_FLAG_NONE, &defaultDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&destination)), context))
    return {};
  void *mapped{};
  const CD3DX12_RANGE readRange(0, 0);
  if (DXFailed(source->Map(0, &readRange, &mapped), context))
    return {};
  memcpy(mapped, data, bytes);
  source->Unmap(0, nullptr);
  commandList->CopyBufferRegion(destination.Get(), 0, source.Get(), 0, bytes);
  const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(destination.Get(), D3D12_RESOURCE_STATE_COPY_DEST, PROBE_READ_STATE);
  commandList->ResourceBarrier(1, &barrier);
  staging.push_back(std::move(source));
  return destination;
}
auto DXProbeVolume::BuildOctree(const std::vector<DXProbeGeometry> &geometry) -> DXProbeOctree {
  DXProbeOctree built;
  std::vector<DXProbeCluster> clusters;
  {
    KUKI_PROFILE_SCOPE("CollectClusters");
    for (const auto &item : geometry) {
      if (item.vertices.empty())
        continue;
      const auto *key = static_cast<const void *>(item.vertices.data());
      auto it = meshClusters.find(key);
      if (it == meshClusters.end() || it->second.vertexCount != item.vertices.size() || it->second.indexCount != item.indices.size())
        it = meshClusters.insert_or_assign(key, MeshClusters{.clusters = BuildClusters(item), .vertexCount = item.vertices.size(), .indexCount = item.indices.size()}).first;
      clusters.reserve(clusters.size() + it->second.clusters.size());
      for (const auto &cluster : it->second.clusters)
        clusters.push_back(TransformCluster(cluster, item.transform));
    }
  }
  if (clusters.empty())
    return built;
  auto low = clusters.front().low;
  auto high = clusters.front().high;
  auto triangles = uint64_t{};
  for (const auto &cluster : clusters) {
    low = glm::min(low, cluster.low);
    high = glm::max(high, cluster.high);
    triangles += cluster.triangles;
  }
  const auto center = (low + high) * .5f;
  const auto span = high - low;
  const auto half = std::max({span.x, span.y, span.z, .01f}) * .5f * 1.02f;
  built.origin = center - glm::vec3(half);
  built.side = half * 2.f;
  built.triangleCount = static_cast<uint32_t>(triangles);
  built.clusterCount = static_cast<uint32_t>(clusters.size());
  std::vector<BuildNode> nodes;
  nodes.reserve(1024);
  BuildNode root;
  root.center = center;
  root.extent = half;
  root.density = static_cast<float>(triangles);
  root.clusters.resize(clusters.size());
  for (uint32_t cluster = 0; cluster < root.clusters.size(); ++cluster)
    root.clusters[cluster] = cluster;
  nodes.push_back(std::move(root));
  {
    KUKI_PROFILE_SCOPE("Subdivide");
    Subdivide(nodes, 0, clusters);
  }
  const auto lattice = 1u << PROBE_OCTREE_MAX_DEPTH;
  const auto cellSize = built.side / static_cast<float>(lattice);
  // one slot per lattice corner rather than a hash map: every corner a leaf can have is one of
  // these, the whole table is 33 cubed entries, and a direct index beats hashing a key that is
  // already a dense coordinate
  const auto corners = static_cast<size_t>(lattice) + 1;
  std::vector<uint32_t> placed(corners * corners * corners, INVALID_NODE);
  const auto Lattice = [corners](const float value) {
    return static_cast<size_t>(std::clamp<long>(std::lround(value), 0, static_cast<long>(corners) - 1));
  };
  const auto CornerProbe = [&](const glm::vec3 &corner) -> uint32_t {
    const auto local = (corner - built.origin) / cellSize;
    const auto x = Lattice(local.x);
    const auto y = Lattice(local.y);
    const auto z = Lattice(local.z);
    const auto key = x + corners * (y + corners * z);
    if (placed[key] != INVALID_NODE)
      return placed[key];
    const auto index = static_cast<uint32_t>(built.probes.size());
    DXProbe probe{};
    probe.position[0] = built.origin.x + static_cast<float>(x) * cellSize;
    probe.position[1] = built.origin.y + static_cast<float>(y) * cellSize;
    probe.position[2] = built.origin.z + static_cast<float>(z) * cellSize;
    probe.anchor[0] = probe.position[0];
    probe.anchor[1] = probe.position[1];
    probe.anchor[2] = probe.position[2];
    // Seeded as though the scene were as far away as the clamp allows, and with no spread, so a
    // fresh probe is visible from everywhere until its own rays say otherwise. Zero would read as a
    // surface at no distance at all, which is total occlusion, and would leave the frames between a
    // build and the first trace with every probe hidden from every point and no bounce anywhere.
    const auto seed = PackHalf(PROBE_DEPTH_RANGE * built.side);
    for (auto &texel : probe.depth)
      texel = seed;
    built.probes.push_back(probe);
    placed[key] = index;
    return index;
  };
  {
    KUKI_PROFILE_SCOPE("PlaceProbes");
    built.nodes.resize(nodes.size());
    for (size_t index = 0; index < nodes.size(); ++index) {
      const auto &node = nodes[index];
      auto &target = built.nodes[index];
      target.center[0] = node.center.x;
      target.center[1] = node.center.y;
      target.center[2] = node.center.z;
      target.extent = node.extent;
      target.depth = node.depth;
      target.leaf = node.leaf ? 1u : 0u;
      for (uint32_t corner = 0; corner < 8; ++corner) {
        target.children[corner] = node.leaf ? INVALID_NODE : node.children[corner];
        if (!node.leaf) {
          target.probes[corner] = INVALID_NODE;
          continue;
        }
        const auto placedProbe = CornerProbe(node.center + CornerOffset(corner, node.extent));
        target.probes[corner] = placedProbe;
        // The same probe corners up to eight leaves, which need not be the same size. It may stray
        // only as far as the smallest of them would tolerate.
        auto &allowance = built.probes[placedProbe].anchor[3];
        const auto granted = PROBE_RELOCATION_LIMIT * node.extent;
        allowance = allowance > 0.f ? std::min(allowance, granted) : granted;
      }
      if (!node.leaf)
        continue;
      ++built.leafCount;
      built.deepestLeaf = std::max(built.deepestLeaf, node.depth);
    }
  }
  {
    // Which side of the geometry each probe stands on, and so whether its own estimate is about the
    // room it is interpolated into or about the void behind a wall.
    KUKI_PROFILE_SCOPE("ProbeSides");
    const auto field = ClassifySpace(geometry, built.origin, built.side, lattice);
    const auto edge = static_cast<int>(lattice);
    for (size_t key = 0; key < placed.size(); ++key) {
      const auto index = placed[key];
      if (index == INVALID_NODE)
        continue;
      const int home[3]{static_cast<int>(key % corners), static_cast<int>(key / corners % corners), static_cast<int>(key / (corners * corners))};
      const glm::vec3 anchor{built.probes[index].anchor[0], built.probes[index].anchor[1], built.probes[index].anchor[2]};
      // A probe sits on a cell corner, so up to eight cells touch it and each has something to say.
      // Direct evidence first: a cell holding surface answers for itself, by which side of that
      // surface the probe is on, which is the wall beside the probe rather than whichever wall was
      // nearest to some cell's centre. Only where nothing touching the probe holds any geometry at
      // all does the fill's verdict stand in, and that is the case the fill exists for.
      auto direct = 0.f;
      auto spread = 0;
      for (auto z = home[2] - 1; z <= home[2]; ++z)
        for (auto y = home[1] - 1; y <= home[1]; ++y)
          for (auto x = home[0] - 1; x <= home[0]; ++x) {
            if (x < 0 || y < 0 || z < 0 || x >= edge || y >= edge || z >= edge)
              continue;
            const auto at = static_cast<size_t>(x) + lattice * (static_cast<size_t>(y) + static_cast<size_t>(lattice) * static_cast<size_t>(z));
            const auto &held = field.surface[at];
            if (field.side[at] == static_cast<uint8_t>(CellSide::Solid)) {
              if (held.area > 0.f && glm::dot(held.normal, held.normal) > 0.f)
                direct += glm::dot(glm::normalize(held.normal), anchor - held.point / held.area) < 0.f ? -held.area : held.area;
              continue;
            }
            if (field.side[at] == static_cast<uint8_t>(CellSide::Behind))
              --spread;
            else if (field.side[at] == static_cast<uint8_t>(CellSide::Facing))
              ++spread;
          }
      built.probes[index].exterior = (direct != 0.f ? direct < 0.f : spread < 0) ? 1.f : 0.f;
    }
  }
  {
    // Each probe's neighbour along each face. Probes sit on lattice corners at whatever stride their
    // leaf's depth implies, so the neighbour is not at a fixed offset: the search steps outward until
    // it finds an occupied corner, and gives up past the stride of the coarsest leaf allowed, which
    // is the furthest a genuine neighbour can be.
    KUKI_PROFILE_SCOPE("ProbeNeighbours");
    const auto reach = static_cast<long>(lattice >> PROBE_OCTREE_MIN_DEPTH);
    const auto span = static_cast<long>(corners);
    for (size_t key = 0; key < placed.size(); ++key) {
      const auto index = placed[key];
      if (index == INVALID_NODE)
        continue;
      const long home[3]{static_cast<long>(key % corners), static_cast<long>((key / corners) % corners), static_cast<long>(key / (corners * corners))};
      auto &probe = built.probes[index];
      for (uint32_t face = 0; face < PROBE_NEIGHBOURS; ++face) {
        probe.neighbours[face] = INVALID_NODE;
        const auto axis = face >> 1;
        const long step = (face & 1) ? 1 : -1;
        for (long distance = 1; distance <= reach; ++distance) {
          long at[3]{home[0], home[1], home[2]};
          at[axis] += step * distance;
          if (at[axis] < 0 || at[axis] >= span)
            break;
          const auto neighbour = placed[static_cast<size_t>(at[0]) + corners * (static_cast<size_t>(at[1]) + corners * static_cast<size_t>(at[2]))];
          if (neighbour == INVALID_NODE)
            continue;
          probe.neighbours[face] = neighbour;
          break;
        }
      }
    }
  }
  KUKI_PROFILE_SCOPE("LookupGrid");
  built.lookup.resize(static_cast<size_t>(lattice) * lattice * lattice);
  for (uint32_t z = 0; z < lattice; ++z)
    for (uint32_t y = 0; y < lattice; ++y)
      for (uint32_t x = 0; x < lattice; ++x) {
        const glm::vec3 point{built.origin.x + (static_cast<float>(x) + .5f) * cellSize, built.origin.y + (static_cast<float>(y) + .5f) * cellSize, built.origin.z + (static_cast<float>(z) + .5f) * cellSize};
        built.lookup[x + lattice * (y + static_cast<size_t>(lattice) * z)] = DescendToLeaf(nodes, point);
      }
  return built;
}
auto DXProbeVolume::Build(DXContext &context, const std::vector<DXProbeGeometry> &geometry) -> bool {
  owner = &context;
  const auto hash = HashGeometry(geometry);
  if (ready && hash == geometryHash) {
    pendingFrames = 0;
    return true;
  }
  // an established volume waits for the geometry to stop moving; the first one cannot wait
  if (ready) {
    if (hash != pendingHash) {
      pendingHash = hash;
      pendingFrames = 0;
      return true;
    }
    if (++pendingFrames < PROBE_REBUILD_SETTLE_FRAMES)
      return true;
  }
  KUKI_PROFILE_SCOPE("ProbeVolume::Build");
  ready = false;
  pendingFrames = 0;
  pendingHash = hash;
  geometryHash = hash;
  const auto built = BuildOctree(geometry);
  if (built.nodes.empty() || built.probes.empty())
    return false;
  origin = built.origin;
  side = built.side;
  leafCount = built.leafCount;
  deepestLeaf = built.deepestLeaf;
  KUKI_PROFILE_MARK("probe volume rebuilt");
  KUKI_PROFILE_SCOPE("Upload");
  // Handed over rather than dropped. These three are read by the scene pass of every frame, and a
  // rebuild happens in the middle of one -- so at this point the two frames behind it are still in
  // flight and still reading whatever is being replaced. Assigning over a `ComPtr` releases what it
  // held there and then, which hands the memory back to the driver while the GPU is inside it: the
  // rebuild survives, and the frame after it faults on an address that no longer belongs to anyone.
  //
  // Retiring instead keeps each one alive until the fence says every frame that could name it has
  // finished. See `DXContext::RetireResource`, and `DXRenderer::EnsureInstanceCapacity`, which grows
  // the instance arena mid-frame for the same reason and takes the same way out.
  context.RetireResource(std::move(nodeBuffer));
  context.RetireResource(std::move(probeBuffer));
  context.RetireResource(std::move(lookupBuffer));
  nodeBuffer = UploadBuffer(built.nodes.data(), built.nodes.size() * sizeof(DXOctreeNode), D3D12_RESOURCE_FLAG_NONE, "CreateCommittedResource for the probe octree");
  probeBuffer = UploadBuffer(built.probes.data(), built.probes.size() * sizeof(DXProbe), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, "CreateCommittedResource for the probes");
  lookupBuffer = UploadBuffer(built.lookup.data(), built.lookup.size() * sizeof(uint32_t), D3D12_RESOURCE_FLAG_NONE, "CreateCommittedResource for the probe lookup grid");
  context.FlushCommandList();
  staging.clear();
  if (!nodeBuffer || !probeBuffer || !lookupBuffer)
    return false;
  nodeCount = static_cast<uint32_t>(built.nodes.size());
  probeCount = static_cast<uint32_t>(built.probes.size());
  probeState = PROBE_READ_STATE;
  tracedFrames = 0;
  // the probes came back black, so there is no mean left to keep and no reason to wind one back
  tracedSamples = 0;
  traceHash = 0;
  ready = true;
  if (probeCount != loggedProbeCount) {
    loggedProbeCount = probeCount;
    const auto bytes = built.nodes.size() * sizeof(DXOctreeNode) + built.probes.size() * sizeof(DXProbe) + built.lookup.size() * sizeof(uint32_t);
    spdlog::info("[DX12] Probe volume: {} probes in {} leaves of {} nodes, deepest {} of {}, over {:.2f} units", probeCount, leafCount, nodeCount, deepestLeaf, PROBE_OCTREE_MAX_DEPTH, side);
    spdlog::info("[DX12] Probe volume: {} triangles in {} clusters, {} cell lookup grid, {:.2f} MB resident", built.triangleCount, built.clusterCount, GetLookupResolution(), static_cast<double>(bytes) / (1024. * 1024.));
  }
  return true;
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
  if (validated || !ready)
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
  constants.grid[2] = probeCount;
  constants.grid[3] = nodeCount;
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
  constants.counts[0] = probeCount;
  constants.counts[1] = nodeCount;
  // carried only so a capture shows how converged the field taken in that frame was
  constants.counts[2] = tracedSamples;
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
  commandList->Dispatch(probeCount, 1, 1);
  const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(probeBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, PROBE_READ_STATE);
  commandList->ResourceBarrier(1, &barrier);
  probeState = PROBE_READ_STATE;
  ++tracedFrames;
  ++tracedSamples;
}
auto DXProbeVolume::Report(DXContext &context) -> void {
  if (reported || !ready || tracedFrames < REPORT_AFTER_FRAMES)
    return;
  reported = true;
  auto *device = context.GetDevice();
  auto *commandList = context.GetCommandList();
  if (!device || !commandList)
    return;
  const auto bytes = static_cast<uint64_t>(probeCount) * sizeof(DXProbe);
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
  for (uint32_t index = 0; index < probeCount; ++index) {
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
  // What the build's own classification found, against what the probes' rays could work out for
  // themselves. The gap between this and the sealed count is the population the trace is blind to:
  // probes outside the scene with nothing in sight, which no ray of theirs can report on.
  if (outside > 0)
    spdlog::info("[DX12] Probe trace: the build placed {} of {} probes behind the scene's surfaces, and those take their neighbours' estimate whatever their own view", outside, probeCount);
  else
    spdlog::info("[DX12] Probe trace: the build found every probe in front of the geometry, so each is judged on its own rays alone");
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
  return probeCount;
}
auto DXProbeVolume::GetNodeCount() const -> uint32_t {
  return nodeCount;
}
auto DXProbeVolume::GetLookupResolution() const -> uint32_t {
  return 1u << PROBE_OCTREE_MAX_DEPTH;
}
auto DXProbeVolume::IsReady() const -> bool {
  return ready;
}
auto DXProbeVolume::Clear() -> void {
  if (owner)
    owner->WaitForGPU();
  staging.clear();
  nodeBuffer.Reset();
  probeBuffer.Reset();
  lookupBuffer.Reset();
  auditSeed.Reset();
  auditResult.Reset();
  auditReadback.Reset();
  probeReadback.Reset();
  auditSeedData = nullptr;
  probeState = PROBE_READ_STATE;
  origin = {};
  side = 0.f;
  geometryHash = 0;
  pendingHash = 0;
  pendingFrames = 0;
  meshClusters.clear();
  nodeCount = 0;
  probeCount = 0;
  leafCount = 0;
  deepestLeaf = 0;
  loggedProbeCount = 0;
  tracedFrames = 0;
  tracedSamples = 0;
  traceHash = 0;
  ready = false;
  validated = false;
  reported = false;
}
} // namespace kuki
#endif
