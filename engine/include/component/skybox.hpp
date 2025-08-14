#pragma once
#include "component.hpp"
#include <glm/ext/vector_uint3.hpp>
#include <kuki_engine_export.h>
namespace kuki {
/// @brief Texture IDs associated with a skybox component
struct KUKI_ENGINE_API Skybox final : public IComponent {
  Skybox()
    : IComponent(std::in_place_type<Skybox>) {}
  int skybox{};
  int irradiance{};
  int prefilter{};
  int brdf{};
  int preview{};
  void CopyTo(Skybox&) const;
};
} // namespace kuki
