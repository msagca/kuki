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
  glm::vec3 specular{1.0f};
  // attenuation terms (for point light)
  float constant{1.0f};
  float linear{.09f};
  float quadratic{.032f};
  auto GetTransform() const -> Transform;
  auto SetTransform(const Transform &) -> void;
};
} // namespace kuki
