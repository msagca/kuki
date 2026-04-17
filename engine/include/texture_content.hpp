#pragma once
#include <bitset>
#include <cstdint>
namespace kuki {
enum class TextureContent : uint8_t {
  Albedo,
  Emissive,
  Metalness,
  Normal,
  Occlusion,
  Roughness,
  Specular
};
using TextureMask = std::bitset<static_cast<uint8_t>(TextureContent::Specular) + 1>;
} // namespace kuki
