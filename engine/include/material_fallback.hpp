#pragma once
#include <glm/vec4.hpp>
#include <texture_content.hpp>
namespace kuki {
struct MaterialFallback {
  glm::vec4 albedo{1.0f};
  glm::vec4 specular{.0f};
  glm::vec4 emissive{.0f};
  float metalness{.5f};
  float occlusion{1.0f};
  float roughness{.5f};
  TextureMask textureMask{0};
};
} // namespace kuki
