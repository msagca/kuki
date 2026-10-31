#pragma once
#include <glm/ext/vector_float2.hpp>
namespace kuki {
/// @brief The wind over a scene, and the waves it raises: what bends the grass and the trees and
/// moves the water.
///
/// Read by the renderer and never written by it. A surface takes as much of it as its material
/// asks for -- `MaterialFallback::sway` for what bends, `MaterialFallback::waves` for what heaves
/// -- and the vertex shader moves it there, in the scene, the shadow maps and the depth prepass
/// alike, so a swaying tree's shadow sways with it and its outline stays on it.
///
/// Two motions make the bend. The wind leans everything that sways one way, steadily, by
/// `strength`; and gusts roll across the ground along the wind, bands `gustScale` units apart
/// moving at `gustSpeed`, each pressing harder for as long as it passes. That travelling band is
/// what a field of grass under wind looks like, and the thing a uniform sway never gets: the eye
/// reads the gust by watching it cross the field. A shiver at `flutter` a second rides on both, out
/// of step from one thing to the next.
///
/// The waves are three trains of sine waves, the longest running with the wind and two shorter
/// across it at angles, so the sea never shows one wave repeating.
///
/// The particles and the clouds have winds of their own -- an emitter's `gravity` and the clouds'
/// `windDirection` -- for a game to set from this one if it wants them all to agree.
///
/// A scene singleton, and saved: the weather a scene was left in is part of it. Drawn where the
/// backend says so (`RendererCapabilities::wind`).
struct Wind {
  /// @brief Which way it blows along the ground, as x and z. Need not be of unit length.
  glm::vec2 direction{1.f, .3f};
  /// @brief How hard it leans what sways, as a scale on each material's `sway`.
  float strength{1.f};
  /// @brief How much harder than that a gust presses at its peak, and how far apart gusts are and
  /// how fast they cross the ground, in units and units a second.
  float gustStrength{.8f};
  float gustScale{18.f};
  float gustSpeed{7.f};
  /// @brief How many times a second what sways shivers, and how far, as a share of its lean.
  float flutter{2.2f};
  float flutterAmount{.35f};
  /// @brief How high the waves stand, from trough to crest, in units, times each material's `waves`.
  float waveHeight{.12f};
  /// @brief How far apart the longest crests are, in units, and how fast they travel, in units a second.
  float waveLength{7.f};
  float waveSpeed{1.6f};
  auto operator==(const Wind &) const -> bool = default;
};
} // namespace kuki
