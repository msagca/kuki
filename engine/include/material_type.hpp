#pragma once
#include <cstdint>
namespace kuki {
enum class MaterialType : uint8_t {
  Lit,
  Unlit
};
/// @brief How a material's alpha is interpreted, following the glTF 2.0 alpha modes.
///
/// Alpha means three unrelated things depending on this, which is why it cannot be inferred from
/// the value alone. `Opaque` ignores it outright, so a texture carrying junk in its fourth channel
/// cannot make a wall see-through. `Mask` is a per-pixel cutout at `alphaCutoff`, which is what
/// foliage and chain-link want: the surface is either fully there or not there at all, so it still
/// writes depth and needs no sorting. `Blend` is genuine translucency, and the only one that has
/// to be drawn after everything opaque, back to front, without writing depth.
enum class AlphaMode : uint8_t {
  Opaque,
  Mask,
  Blend
};
enum class MaterialProperty : uint8_t {
  AlbedoTexture,
  NormalTexture,
  MetalnessTexture,
  OcclusionTexture,
  RoughnessTexture,
  SpecularTexture,
  EmissiveTexture
};
} // namespace kuki
