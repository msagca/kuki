#pragma once
#include "component.hpp"
#include <glm/ext/vector_float3.hpp>
#include <kuki_engine_export.h>
#include <vector>
namespace kuki {
struct KUKI_ENGINE_API Light final : IComponent {
  LightType type{LightType::Directional};
  glm::vec3 vector{1.0f, 3.0f, 2.0f};
  glm::vec3 ambient{.2f};
  glm::vec3 diffuse{.5f};
  glm::vec3 specular{1.0f};
  // attenuation terms (for point light)
  float constant{1.0f};
  float linear{.09f};
  float quadratic{.032f};
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
  Transform GetTransform() const;
  void SetTransform(const Transform&);
};
} // namespace kuki
