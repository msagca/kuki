#pragma once
#include "component.hpp"
#include <engine_export.h>
#include <glad/glad.h>
#include <vector>
struct ENGINE_API Mesh : IComponent {
  unsigned int vertexArray{};
  unsigned int vertexBuffer{};
  unsigned int indexBuffer{};
  int vertexCount{}; // NOTE: includes duplicates if no EBO is used
  int indexCount{};
  /// <summary>
  /// Minimum bounds of the mesh at scale 1
  /// </summary>
  glm::vec3 minBound{};
  /// <summary>
  /// Maximum bounds of the mesh at scale 1
  /// </summary>
  glm::vec3 maxBound{};
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
