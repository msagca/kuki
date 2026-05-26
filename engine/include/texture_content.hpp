#pragma once
#include <bitset>
#include <cstdint>
namespace kuki {
enum class TextureContent : uint8_t {
  // NOTE: do not change the order here, otherwise the lit shader won't function correctly
  Albedo,
  Normal,
  Metalness,
  Occlusion,
  Roughness,
  Specular,
  Emissive,
  Skybox
};
using TextureMask = std::bitset<static_cast<uint8_t>(TextureContent::Skybox) + 1>;
} // namespace kuki
