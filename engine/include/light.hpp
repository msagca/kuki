#pragma once
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <light_type.hpp>
#include <transform.hpp>
namespace kuki {
struct KUKI_ENGINE_API Light {
  LightType type{LightType::Directional};
  glm::vec3 position{};
  glm::quat rotation{1.f, .0f, .0f, .0f};
  glm::vec3 ambient{.2f};
  glm::vec3 diffuse{.5f};
  glm::vec3 specular{1.f};
  // point light
  float constant{1.f};
  float linear{.09f};
  float quadratic{.032f};
  // spot light
  float innerCutoff{.91f}; // cos(25)
  float outerCutoff{.82f}; // cos(35)
  auto GetModel() const -> glm::mat4;
  auto GetTransform() const -> Transform;
  auto GetView() const -> glm::mat4;
  auto SetTransform(const Transform &) -> void;
};
} // namespace kuki
