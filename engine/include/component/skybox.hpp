#pragma once
#include "component.hpp"
#include <kuki_engine_export.h>
#include <glm/ext/vector_uint3.hpp>
#include <vector>
namespace kuki {
/// @brief Texture IDs associated with a skybox component
struct KUKI_ENGINE_API Skybox final : IComponent {
  SkyboxData data{};
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
} // namespace kuki
