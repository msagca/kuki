#pragma once
#include <bounding_box.hpp>
#include <buffer_object.hpp>
#include <kuki_engine_export.h>
namespace kuki {
struct KUKI_ENGINE_API GLMesh final : public BufferObject {
  unsigned int vao{};
  unsigned int ebo{};
  /// @brief The vertex buffer behind `vao`, kept only so it can be deleted.
  ///
  /// Nothing draws through this -- the vertex array records the binding, which is what a draw
  /// reads. It is stored because deleting the vertex array does not delete the buffer bound to it,
  /// so a mesh with no record of its own buffer is a mesh that cannot be freed.
  unsigned int vbo{};
  int vertexCount{};
  int indexCount{};
  bool skinned{};
  auto operator==(const GLMesh &) const -> bool;
  explicit operator bool() const;
};
} // namespace kuki
namespace std {
template <>
struct hash<kuki::GLMesh> {
  auto operator()(const kuki::GLMesh &mesh) const noexcept -> size_t {
    return hash<int>{}(mesh.vao);
  }
};
} // namespace std
