#pragma once
#include <bounding_box.hpp>
#include <component.hpp>
#include <kuki_engine_export.h>
namespace kuki {
struct KUKI_ENGINE_API Mesh final : public IComponent {
  Mesh();
  int vao{};
  int ebo{};
  /// @brief Number of vertices in the mesh; may include duplicates if no EBO is used
  int vertexCount{};
  int indexCount{};
  BoundingBox bounds{};
};
} // namespace kuki
