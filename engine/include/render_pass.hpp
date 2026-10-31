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
  /// @brief The lines a cel is inked with, found in the depth buffer and drawn over the resolved scene.
  ///
  /// Ahead of the particles and the air rather than at the end of the chain, both on purpose. The
  /// lines come from the depth of solid things, and a particle has none to give: drawn after the
  /// particles, every raindrop in front of a figure would be crossed by the figure's outline. And the
  /// fog that follows hazes a distant line as it hazes the wall the line is drawn on, which is what
  /// keeps a far backdrop from being drawn with the same weight of ink as the figures in front of it.
  Ink,
  Outline,
  /// @brief Text drawn over the finished picture, in the window's own pixels.
  ///
  /// Last in the chain and after tone mapping on purpose: a caption is not part of the shot. It is
  /// not lit, not exposed, and not tone mapped, so it reads the same whatever the scene is doing --
  /// which is what a clock has to do, and what putting it in the scene could not give it.
  Overlay,
  /// @brief Every particle emitter in the scene, moved a step and drawn over the shaded picture.
  ///
  /// After the scene is resolved and before the air is marched, and both halves of that are
  /// deliberate. After, because the quads are blended onto a finished picture and tested against a
  /// depth buffer that has to exist first. Before, because everything downstream of the fog pass is
  /// what makes a spark look like one: the haze dims it with distance and the bloom chain gives it
  /// the glow that says it is bright rather than merely orange.
  Particles,
  /// @brief A level surface mirrored in screen space, where the scene has one. See
  /// `PlanarReflection`.
  ///
  /// After the ink, so a figure's reflection is inked as the figure is, and before the particles,
  /// whose depth is not in the buffer the rays walk: a raindrop is not reflected, and drawn after
  /// this, it falls over the reflection rather than being smeared into it.
  PlanarReflection,
  ProbeTrace,
  Scene,
  ShadowMap,
  SpotShadowMap,
  /// @brief The grade and whatever the game is doing to the whole picture: a flash, an impact frame,
  /// speed lines, a smear from the centre.
  ///
  /// After tone mapping, in display space, because every one of these is a statement about the
  /// picture rather than about the light in the scene. A grade that pushes the darks towards blue
  /// means the darks as they are seen, and a white flash is white on the screen whatever the
  /// exposure was.
  Stylize,
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
