#pragma once
#include "component.hpp"
#include <glm/ext/matrix_float4x4.hpp>
#include <kuki_engine_export.h>
namespace kuki {
struct KUKI_ENGINE_API BoundingBox {
  /// @brief Minimum local bounds of the mesh at scale 1
  glm::vec3 min{std::numeric_limits<float>::max()};
  /// @brief Maximum local bounds of the mesh at scale 1
  glm::vec3 max{std::numeric_limits<float>::lowest()};
  BoundingBox();
  BoundingBox(glm::vec3, glm::vec3);
  /// @brief Get the world space bounds
  BoundingBox GetWorldBounds(glm::mat4);
};
struct KUKI_ENGINE_API Mesh final : public IComponent {
  Mesh()
    : IComponent(std::in_place_type<Mesh>) {}
  int vertexArray{};
  int vertexBuffer{};
  int indexBuffer{};
  int vertexCount{}; // NOTE: includes duplicates if no EBO is used
  int indexCount{};
  BoundingBox bounds{};
  void CopyTo(Mesh&) const;
};
} // namespace kuki
