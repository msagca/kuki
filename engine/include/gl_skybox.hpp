#pragma once
#include <glm/ext/vector_uint3.hpp>
#include <kuki_engine_export.h>
#include <target_description.hpp>
namespace kuki {
struct KUKI_ENGINE_API GLSkybox {
  int skybox{};
  int irradiance{};
  int prefilter{};
  int brdf{};
  TargetDescription desc{.target = TargetType::Cubemap};
};
} // namespace kuki
