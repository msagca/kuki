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
  /// @brief Text drawn over the finished picture, in the window's own pixels.
  ///
  /// Last in the chain and after tone mapping on purpose: a caption is not part of the shot. It is
  /// not lit, not exposed, and not tone mapped, so it reads the same whatever the scene is doing --
  /// which is what a clock has to do, and what putting it in the scene could not give it.
  Overlay,
  ProbeTrace,
  Scene,
  ShadowMap,
  SpotShadowMap,
  ToneMapping,
  /// @brief A cloud layer marched through the atmosphere, where a scene has one of each.
  ///
  /// Gathered at its own resolution rather than the viewport's, which is why it is a pass of its
  /// own rather than a step inside the one below: a cloud is a smooth thing seen across the whole
  /// sky, and marching it per pixel would cost several times what the rest of the frame does.
  VolumetricClouds,
  /// @brief The air between the camera and the scene, and what the lights scatter out of it.
  ///
  /// Last of the three that build the picture's light, and after the clouds deliberately: a shaft
  /// is broken by the cloud that stands in front of the sun, so the cloud has to be known first.
  VolumetricFog
};
/// @brief How many passes the enum names, for anything that keeps one entry per pass.
///
/// Derived from the last enumerator rather than added to the enum as a sentinel, so that a `Count`
/// can never be mistaken for a pass, switched on, or run.
inline constexpr size_t RENDER_PASS_COUNT = static_cast<size_t>(RenderPass::VolumetricFog) + 1;
}
