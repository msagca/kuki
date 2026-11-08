#pragma once
namespace kuki {
/// @brief How much the creases of a scene darken: where a house meets the ground, between trees, in
/// the steps of a terrace. Estimated from the depth prepass and taken out of the indirect light, so a
/// prop settles onto what it stands on where the probe field is too coarse to say it touches it.
///
/// Measured in scene units rather than pixels, so a crease is the same width of shade at any
/// distance: the reach is projected onto the screen per pixel. Where it projects to under a pixel or
/// two the estimate fades out rather than being made from samples closer together than the depth
/// buffer can tell apart -- a crease seen from that far is a line too thin to shade.
///
/// The sun's light is left alone by default, since its shadow map already knows what it reaches;
/// `direct` lends some of the darkening to it, for a softer, more drawn look where things touch.
///
/// A scene singleton, and saved: how soft the shade in a scene's corners is belongs to the scene.
/// Nought `intensity`, or no such component, stands the pass down to a cleared target.
struct AmbientOcclusion {
  /// @brief How far round a point is looked for something that hides the sky from it, in scene units.
  float radius{.6f};
  /// @brief How dark a crease goes, from nought (off) to about two.
  float intensity{1.f};
  /// @brief Bends the darkening: above one keeps it to the deepest corners, below spreads it.
  float power{1.2f};
  /// @brief How much of the darkening the direct light takes as well, from nought to one.
  float direct{.0f};
  auto operator==(const AmbientOcclusion &) const -> bool = default;
};
} // namespace kuki
