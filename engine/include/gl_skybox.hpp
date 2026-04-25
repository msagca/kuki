#pragma once
#include <target_description.hpp>
namespace kuki {
struct GLSkybox {
  TargetDescription desc{};
  unsigned int brdf{};
  unsigned int irradiance{};
  unsigned int prefilter{};
  unsigned int skybox{};
};
} // namespace kuki
