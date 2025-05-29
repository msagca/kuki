#pragma once
#include "component.hpp"
#include <kuki_engine_export.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <vector>
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
struct KUKI_ENGINE_API Mesh final : IComponent {
  int vertexArray{};
  int vertexBuffer{};
  int indexBuffer{};
  int vertexCount{}; // NOTE: includes duplicates if no EBO is used
  int indexCount{};
  BoundingBox bounds{};
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
} // namespace kuki
