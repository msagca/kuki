#pragma once
#include "component.hpp"
#include "texture.hpp"
#include <kuki_engine_export.h>
#include <glm/ext/vector_uint3.hpp>
#include <vector>
namespace kuki {
struct KUKI_ENGINE_API Skybox final : IComponent {
  glm::uvec3 id{}; // skybox (x) + irradiance map (y) + preview (z) texture IDs
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
} // namespace kuki
