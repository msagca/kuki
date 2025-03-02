#pragma once
#include "component.hpp"
#include <engine_export.h>
#include <glm/ext/vector_float3.hpp>
#include <vector>
struct ENGINE_API Transform : IComponent {
  glm::vec3 position{};
  glm::vec3 rotation{}; // NOTE: these should be in radians and converted to degrees when displayed in the editor
  glm::vec3 scale{1.0f};
  int parent{-1};
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
