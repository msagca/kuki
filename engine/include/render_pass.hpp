#pragma once
#include <cstddef>
#include <cstdint>
namespace kuki {
enum class RenderPass : uint8_t {
  AntiAliasing,
  BloomEffect,
  BlurEffect,
  BrightPassFilter,
  DepthPrepass,
  Outline,
  ProbeTrace,
  Scene,
  ShadowMap,
  SpotShadowMap,
  ToneMapping
};
/// @brief How many passes the enum names, for anything that keeps one entry per pass.
///
/// Derived from the last enumerator rather than added to the enum as a sentinel, so that a `Count`
/// can never be mistaken for a pass, switched on, or run.
inline constexpr size_t RENDER_PASS_COUNT = static_cast<size_t>(RenderPass::ToneMapping) + 1;
}
