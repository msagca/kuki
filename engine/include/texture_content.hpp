#pragma once
#include <bitset>
#include <cstdint>
namespace kuki {
/// @brief What a texture supplies to the shading model, and its bit position in a `TextureMask`.
///
/// The order is load-bearing: the lit shaders test the mask by literal bit, so reordering these
/// silently rebinds every texture slot rather than failing to compile.
enum class TextureContent : uint8_t {
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
