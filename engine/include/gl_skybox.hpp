#pragma once
#include <glm/ext/vector_uint3.hpp>
#include <target_description.hpp>
namespace kuki {
struct GLSkybox {
  int skybox{};
  int irradiance{};
  int prefilter{};
  int brdf{};
  TargetDescription desc{.target = TargetType::Cubemap};
};
} // namespace kuki
