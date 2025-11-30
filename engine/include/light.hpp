#pragma once
#include <component.hpp>
#include <glm/ext/vector_float3.hpp>
#include <kuki_engine_export.h>
#include <transform.hpp>
namespace kuki {
struct KUKI_ENGINE_API Light final : public IComponent {
  Light();
  Light(const Light&);
  Light& operator=(const Light&);
  LightType type{LightType::Directional};
  glm::vec3 vector{3.0f};
  glm::vec3 ambient{.2f};
  glm::vec3 diffuse{.5f};
  glm::vec3 specular{1.0f};
  // attenuation terms (for point light)
  float constant{1.0f};
  float linear{.09f};
  float quadratic{.032f};
  Transform GetTransform() const;
  void SetTransform(const Transform&);
};
} // namespace kuki
