#pragma once
#include "component.hpp"
#include <engine_export.h>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
struct ENGINE_API Transform : IComponent {
  glm::vec3 position{};
  glm::quat rotation{1.0f, .0f, .0f, .0f};
  glm::vec3 scale{1.0f};
  int parent{-1};
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
