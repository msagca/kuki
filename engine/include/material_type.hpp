#pragma once
#include <cstdint>
namespace kuki {
enum class MaterialType : uint8_t {
  Bloom,
  Blur,
  BrightPass,
  GammaCorrect,
  Lit,
  Skybox,
  Unlit,
  Unknown
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
