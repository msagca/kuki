#pragma once
#include "component.hpp"
#include <glad/glad.h>
#include <kuki_export.h>
#include <vector>
namespace kuki {
struct KUKI_API BoundingBox {
  /// @brief Minimum local bounds of the mesh at scale 1
  glm::vec3 min{};
  /// @brief Maximum local bounds of the mesh at scale 1
  glm::vec3 max{};
  BoundingBox();
  BoundingBox(glm::vec3, glm::vec3);
  /// @brief Get the world space bounds
  BoundingBox GetWorldBounds(const Transform*);
};
struct KUKI_API Mesh final : IComponent {
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
