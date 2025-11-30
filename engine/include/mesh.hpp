#pragma once
#include <component.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <kuki_engine_export.h>
namespace kuki {
struct KUKI_ENGINE_API BoundingBox {
  /// @brief Minimum local bounds of the mesh at scale 1
  glm::vec3 min{};
  /// @brief Maximum local bounds of the mesh at scale 1
  glm::vec3 max{};
  BoundingBox();
  BoundingBox(glm::vec3, glm::vec3);
  /// @brief Get the world space bounds
  BoundingBox GetWorldBounds(const glm::mat4&);
};
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
