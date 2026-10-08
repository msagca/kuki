#pragma once
#include <cstdint>
namespace kuki {
/// @brief How many samples the scene is rasterised at before it is resolved for display.
///
/// The order is load-bearing in the same way `ExposureMode`'s is: `EnumTraits<AntiAliasingMode>`
/// names these by index, so inserting one in the middle changes what an existing selection means.
///
/// Multisampling is the only method here because it is the only one the pipeline has: the
/// `AntiAliasing` pass is a resolve of the multisampled scene target rather than a filter with an
/// opinion, on both backends. A post-process method -- an edge-detecting filter, or a temporal
/// accumulation -- would be a different pass rather than a different value of this, which is why
/// this enumerates sample counts and not techniques.
enum class AntiAliasingMode : uint8_t {
  /// @brief One sample a pixel, and the resolve becomes a straight copy.
  ///
  /// Not a broken state: every pass that treats a multisampled target differently already asks the
  /// target how many samples it carries rather than assuming, so a one-sample scene goes through
  /// the frame intact. What it loses besides the smooth edges is the generosity at a silhouette
  /// that picking gets from having four ids under a pixel -- see `pick.frag`.
  None,
  MSAA2x,
  MSAA4x,
  MSAA8x
};
/// @brief Samples a mode asks the scene target for.
inline auto SampleCount(const AntiAliasingMode mode) -> int {
  switch (mode) {
  case AntiAliasingMode::MSAA2x:
    return 2;
  case AntiAliasingMode::MSAA4x:
    return 4;
  case AntiAliasingMode::MSAA8x:
    return 8;
  default:
    return 1;
  }
}
/// @brief What the scene is antialiased with, as a component on an entity.
///
/// A component for the reason `IndirectLighting` is one, and it sits on the same settings entity:
/// this is a property of the scene being drawn rather than of the installation drawing it, so it
/// does not belong in `EngineConfig` beside the tone curve. Two scenes in one session can
/// reasonably disagree about it -- a heavy one dropped to no multisampling and a light one at
/// eight -- which a preference held once per machine cannot express.
///
/// Changing it reallocates the scene target, which is the reason it is a mode and not a free
/// integer: the sample count has to be one the hardware rasterises, and a target asked for three
/// samples fails to allocate with nothing on screen to say why.
///
/// Not in `SERIALIZED_TYPES`, matching `IndirectLighting`. A sample count is the one setting here
/// that is also a cost, and a scene authored on a machine that could afford eight would impose
/// eight on one that cannot. The editor puts a fresh one on the settings entity when a scene has
/// none, so the control is always there; what it is not is a value that travels.
struct AntiAliasing {
  AntiAliasingMode mode{AntiAliasingMode::MSAA4x};
};
} // namespace kuki
