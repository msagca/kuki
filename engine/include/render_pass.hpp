#pragma once
#include <cstdint>
namespace kuki {
enum class RenderPass : uint8_t {
  AntiAliasing,
  BloomEffect,
  BlurEffect,
  BrightPassFilter,
  GammaCorrection,
  Outline,
  Scene,
  ShadowMap,
  SpotShadowMap
};
}
