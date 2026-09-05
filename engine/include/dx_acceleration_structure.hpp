#pragma once
#ifdef KUKI_HAS_DIRECTX
#include <cstdint>
#include <dx_common.hpp>
#include <dx_descriptor_heap.hpp>
#include <dx_mesh.hpp>
#include <kuki_engine_export.h>
#include <unordered_map>
#include <vector>
namespace kuki {
class DXContext;
class DXPipelineCache;
/// @brief What a traced ray needs to know about the surface it hit, found by the hit's instance id.
///
/// A hit reports an instance, a geometry and a primitive, and nothing else. None of the three says
/// where the vertices are or what the surface is made of, so the shader has to look that up, and
/// this is the record it looks up. One per instance in the top-level structure, addressed by the
/// `InstanceID` the build wrote, which is why the array is parallel to the instance descriptions.
///
/// Every field but the entity id is an index into the shader-visible descriptor heap, to be reached
/// through an unbounded array. Storing indices rather than addresses is what lets one record serve
/// a buffer and a texture alike: the heap is the single place all of them already live.
///
/// `indexBuffer` is `DXDescriptorHeap::InvalidIndex` for geometry that carries no index buffer, in
/// which case its vertices are a triangle list and the primitive index addresses them directly.
///
/// The material's scalar values ride along rather than being looked up. A texture table index is
/// enough only for a material that has textures, and most do not: an untextured surface is a colour
/// and nothing else, and a trace that resolved only the table would shade every wall of a Cornell
/// box the same white. `textureMask` says which of the seven slots are real, exactly as
/// `MaterialFallback::textureMask` does for the raster path.
///
/// The transparency values ride along for the same reason, and are what lets a ray decide whether a
/// surface it reached actually stops it. Without them every surface is a wall: a cutout leaf casts
/// the shadow of its quad, and a pane of glass casts the shadow of a brick.
///
/// Mirrored by the `GeometryInfo` struct in every shader that reads this buffer, field for field.
/// A structured buffer's stride comes from the type, so a field added on one side and not the other
/// misaligns every record after the first rather than failing to compile.
struct DXGeometryInfo {
  uint32_t vertexBuffer{};
  uint32_t indexBuffer{};
  uint32_t materialTextures{};
  uint32_t textureMask{};
  float albedo[4]{};
  float emissive[4]{};
  float attenuation[4]{};
  float transmission{};
  float thickness{};
  float alphaCutoff{};
  uint32_t alphaMode{};
  uint32_t entityId{};
  uint32_t padding[3]{};
};
/// @brief One placement of a mesh in the traced scene, as `DXAccelerationStructure::Build` takes it.
///
/// The transform is a world matrix in the column-major order glm stores it in; the build transposes
/// it into the row-major three-by-four form the instance description wants.
struct DXRayInstance {
  const DXMesh *mesh{};
  float transform[16]{};
  float albedo[4]{1.f, 1.f, 1.f, 1.f};
  float emissive[4]{};
  float attenuation[4]{1.f, 1.f, 1.f, .0f};
  float transmission{};
  float thickness{};
  float alphaCutoff{.5f};
  uint32_t alphaMode{};
  uint32_t materialTextures{DXDescriptorHeap::InvalidIndex};
  uint32_t textureMask{};
  uint32_t entityId{};
};
/// @brief The scene as a ray sees it: one bottom-level structure per mesh under one top-level one.
///
/// Rasterising and tracing want the same geometry organised differently. A draw call binds one
/// mesh and lets the instance buffer place it many times; a ray has no draw call to belong to, so
/// every placement has to exist in a structure the traversal hardware can walk. That structure is
/// two-level: a bottom-level structure holds a mesh's triangles in its own object space and is
/// built once, and a top-level structure holds a transform and a bottom-level pointer per
/// placement and is rebuilt whenever anything moves.
///
/// The bottom-level structures are cached on the mesh they were built from, so a mesh drawn a
/// hundred times is still only built once. The top-level structure is rebuilt every frame, which
/// is cheap next to a bottom-level build and is the only way a moving entity ends up where it is
/// rather than where it was.
///
/// Skinned meshes are left out. Their vertex buffers hold the bind pose and the skinning happens
/// in the vertex shader, so a structure built from those buffers would put the geometry wherever
/// the artist modelled it rather than wherever the animation currently has it.
///
/// TODO: skin into a posed vertex buffer with a compute pass, then rebuild those meshes per frame
class KUKI_ENGINE_API DXAccelerationStructure final {
public:
  /// @brief Builds whatever is missing and rebuilds the top-level structure over the instances.
  ///
  /// Records into the frame's command list, so it must run before any pass binds pipeline state:
  /// growing a buffer waits on the GPU, and the barriers it emits are easier to reason about with
  /// nothing else in flight.
  ///
  /// Instances whose mesh is skinned, empty, or has no bottom-level structure are dropped rather
  /// than failing the build, so one bad mesh costs its own geometry and not the whole scene.
  ///
  /// @return False when the device cannot trace, when nothing was left to place, or when a
  /// resource could not be created; in each case the structures are unusable and `IsReady` is false.
  auto Build(DXContext &, const std::vector<DXRayInstance> &) -> bool;
  /// @brief Traces a coarse grid of camera rays once and logs what came back.
  ///
  /// A structure built from wrong transforms builds without complaint and traverses without
  /// complaint; it simply reports hits in the wrong places, or none at all. Nothing else in the
  /// backend reads the structures yet, so without this the build is unverifiable and a mistake in
  /// it would surface much later, wearing the costume of a lighting bug.
  ///
  /// Runs at most once per scene, and stalls the pipeline to read its results back. Must be called
  /// where a stall is allowed, which is the same place `Build` is: before any pass binds state.
  ///
  /// @param inverseViewProjection Inverse of the camera's view-projection, which turns a pixel of
  /// the grid into a world-space ray.
  /// @param position The camera's world position, where every ray starts.
  auto Validate(DXContext &, DXPipelineCache &, const float *, const float *) -> void;
  /// @brief Address of the top-level structure, to bind as a root shader resource view.
  auto GetTopLevelAddress() const -> D3D12_GPU_VIRTUAL_ADDRESS;
  /// @brief Address of this frame's geometry records, parallel to the instances in the structure.
  auto GetGeometryInfoAddress() const -> D3D12_GPU_VIRTUAL_ADDRESS;
  auto GetInstanceCount() const -> uint32_t;
  /// @brief Whether the last build left something a ray can be traced against.
  auto IsReady() const -> bool;
  /// @brief Releases every structure and its descriptors. For scene changes and shutdown.
  ///
  /// Waits on the GPU first: a structure the command list still references cannot be freed, and
  /// the meshes these were built from are usually being torn down in the same breath.
  auto Clear() -> void;
private:
  /// @brief One mesh's bottom-level structure, with the descriptors its geometry is reached through.
  struct MeshStructure {
    ComPtr<ID3D12Resource> structure;
    uint32_t vertexBuffer{DXDescriptorHeap::InvalidIndex};
    uint32_t indexBuffer{DXDescriptorHeap::InvalidIndex};
  };
  DXContext *owner{};
  std::unordered_map<const DXMesh *, MeshStructure> meshStructures;
  ComPtr<ID3D12Resource> topLevel;
  ComPtr<ID3D12Resource> scratch;
  ComPtr<ID3D12Resource> instanceDescs;
  ComPtr<ID3D12Resource> geometryInfo;
  ComPtr<ID3D12Resource> validationSeed;
  ComPtr<ID3D12Resource> validationResult;
  ComPtr<ID3D12Resource> validationReadback;
  uint8_t *instanceDescData{};
  uint8_t *geometryInfoData{};
  uint8_t *validationSeedData{};
  D3D12_GPU_VIRTUAL_ADDRESS geometryInfoAddress{};
  uint64_t scratchBytes{};
  uint64_t topLevelBytes{};
  uint64_t meshStructureBytes{};
  uint32_t capacity{};
  uint32_t instanceCount{};
  bool ready{};
  bool validated{};
  /// @brief Builds a mesh's bottom-level structure and the views its buffers are reached through.
  ///
  /// Geometry is declared opaque here and stays that way, because a bottom-level structure is
  /// cached per mesh while transparency is a property of the material: the same mesh drawn once in
  /// glass and once in stone would need two answers. The instance description carries the override
  /// instead, so `Build` can force an individual placement non-opaque and make traversal pause
  /// there to ask the shader whether the hit counts.
  ///
  /// @param scratchAddress Scratch space at least as large as the prebuild info asked for.
  auto BuildMeshStructure(const DXMesh *, const D3D12_GPU_VIRTUAL_ADDRESS) -> bool;
  /// @brief Describes a mesh's triangles, or returns false when it has none to describe.
  auto DescribeGeometry(const DXMesh *, D3D12_RAYTRACING_GEOMETRY_DESC &) const -> bool;
  /// @brief Creates a raw buffer view over a mesh buffer, so a shader can fetch from it by offset.
  auto CreateBufferView(ID3D12Resource *, const uint64_t) -> uint32_t;
  /// @brief Grows the per-frame instance and geometry rings to hold at least this many instances.
  ///
  /// One region per frame in flight, because both are written by the processor while the GPU may
  /// still be reading the previous frame's. Growing releases the old buffers, so it waits on the
  /// GPU; it happens when an instance count reaches a new high and then stops.
  auto EnsureCapacity(const uint32_t) -> bool;
  /// @brief Grows the shared scratch buffer, which every build in a frame borrows in turn.
  auto EnsureScratch(const uint64_t) -> bool;
  /// @brief Creates the top-level structure, sized for the current capacity.
  auto EnsureTopLevel(const uint64_t) -> bool;
  /// @brief Creates the seed, result and readback buffers the validation trace reports through.
  auto EnsureValidationBuffers() -> bool;
};
} // namespace kuki
#endif
