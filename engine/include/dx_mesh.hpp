#pragma once
#ifdef KUKI_HAS_DIRECTX
#include <buffer_object.hpp>
#include <dx_common.hpp>
namespace kuki {
/// @brief Vertex and index buffers for one mesh, with the views the input assembler binds.
///
/// Both buffers live in an upload heap so the data can be written once with `Map` and needs no
/// staging copy or barrier. That costs bandwidth on every draw versus a default-heap resource, so
/// static geometry should eventually move.
///
/// TODO: stage through an upload heap into a default heap once meshes are large enough to matter
struct DXMesh final : public BufferObject {
  ComPtr<ID3D12Resource> vertexBuffer;
  ComPtr<ID3D12Resource> indexBuffer;
  D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
  D3D12_INDEX_BUFFER_VIEW indexBufferView{};
  uint32_t vertexCount{};
  uint32_t indexCount{};
  /// @brief Whether the mesh is posed by a bone palette rather than by its entity's transform.
  ///
  /// A skinned mesh needs a different pipeline, a different vertex layout and a per-draw bone
  /// buffer, so it cannot share a batch with a static one even when the geometry is identical.
  bool skinned{};
  explicit operator bool() const {
    return vertexBuffer && vertexCount > 0;
  }
};
} // namespace kuki
#endif
