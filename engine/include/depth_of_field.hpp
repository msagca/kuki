#pragma once
#include <glm/vec2.hpp>
namespace kuki {
/// @brief A shallow depth of field: a band of the scene in focus, and what is nearer or further
/// blurred by how far it is from it. Over a map seen from above, that band runs across the picture
/// and the top and bottom soften away, which is much of what makes the map read as a small model on
/// a table rather than a country seen from a plane -- the eye knows a lens only focuses that
/// shallowly when what it is looking at is close and small.
///
/// The band is measured as a share of the distance focused at, not in scene units, so it stays the
/// same width on the screen as the camera comes closer or draws away and the look holds at every
/// zoom.
///
/// A scene singleton, and saved: how a scene is meant to be seen belongs to it. Nought `maxBlur`, or
/// no such component, stands the pass down to a copy.
struct DepthOfField {
  /// @brief The distance in focus, in scene units. Nought focuses on whatever is at `focusPoint`.
  float focusDistance{.0f};
  /// @brief Where on the picture to focus when `focusDistance` is nought, from the top left (0, 0)
  /// to the bottom right (1, 1). Read on the GPU from the depth there, every frame.
  glm::vec2 focusPoint{.5f, .5f};
  /// @brief How far either side of the focus stays sharp, as a share of the focus distance.
  float focusRange{.12f};
  /// @brief How much further the blur takes to grow to its largest, as a share of the focus distance.
  float transition{.5f};
  /// @brief The largest blur, as a radius in pixels of a picture a thousand and eighty high.
  float maxBlur{6.f};
  auto operator==(const DepthOfField &) const -> bool = default;
};
} // namespace kuki
