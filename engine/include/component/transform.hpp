#pragma once
#include "component.hpp"
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <kuki_export.h>
#include <vector>
struct KUKI_API Transform final : IComponent {
  glm::vec3 position{};
  glm::quat rotation{1.0f, .0f, .0f, .0f};
  glm::vec3 scale{1.0f};
  int parent{-1};
  mutable glm::mat4 model{1.0f};
  mutable bool dirty{true};
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
