#pragma once
#include <bounding_box.hpp>
#include <buffer_object.hpp>
#include <kuki_engine_export.h>
namespace kuki {
struct KUKI_ENGINE_API GLMesh final : public BufferObject {
  unsigned int vao{};
  unsigned int ebo{};
  int vertexCount{};
  int indexCount{};
  BoundingBox bounds{};
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
