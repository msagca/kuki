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
  glm::vec3 forward{.0f, .0f, -1.f};
  glm::vec3 up{.0f, 1.f, .0f};
  glm::vec3 right{1.f, .0f, .0f};
  glm::vec3 ambient{.2f};
  glm::vec3 diffuse{.5f};
  glm::vec3 specular{1.f};
  float intensity{1.f};
  float constant{1.f};
  float linear{.09f};
  float quadratic{.032f};
  float innerCutoff{.91f};
  float outerCutoff{.82f};
  float nearPlane{.1f};
  float farPlane{100.f};
  float orthoSize{10.f};
  auto GetTransform() const -> Transform;
  auto GetView() const -> glm::mat4;
  auto SetRotation(const glm::quat &) -> void;
  auto SetTransform(const Transform &) -> void;
};
} // namespace kuki
