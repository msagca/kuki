#pragma once
#include "component.hpp"
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <kuki_engine_export.h>
#include <vector>
namespace kuki {
struct KUKI_ENGINE_API Transform final : IComponent {
  glm::vec3 position{};
  glm::quat rotation{1.0f, .0f, .0f, .0f};
  glm::vec3 scale{1.0f};
  int parent{-1};
  mutable glm::mat4 local{1.0f};
  mutable glm::mat4 world{1.0f};
  mutable bool localDirty{true};
  mutable bool worldDirty{true};
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
} // namespace kuki
