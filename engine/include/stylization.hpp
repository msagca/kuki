#pragma once
#include <cstdint>
#include <glm/ext/vector_float3.hpp>
namespace kuki {
/// @brief How a scene is drawn when it is meant to look drawn: cel bands, ink lines and a grade.
///
/// Three stages of one look, held together because they are tuned together. A cel band that is too
/// wide wants a heavier line to hold its edge, and a grade that lifts the shadows changes what tint
/// they ought to have been given in the first place. Splitting them into three components would make
/// three things to find in the hierarchy for one decision.
///
/// The bands are a property of the material as much as of the scene: `MaterialFallback::toon` says
/// how far each surface takes them, so a figure can be cel shaded in front of a backdrop that is not.
/// What is here is the shape of the bands for every surface that does. The lines and the grade are
/// the whole picture's, and a scene with none of this component draws no lines and grades nothing.
///
/// A scene singleton: it describes the scene, not the entity it hangs off. Saved with the scene,
/// because the look is part of the work. What happens to the picture for a moment -- a flash, speed
/// lines -- is `ScreenEffects`, which is not.
struct Stylization {
  /// @brief How many tones a lit surface is drawn in. Two is lit and shaded; three adds a midtone.
  uint32_t bands{2};
  /// @brief How wide the step between two tones is, in the cosine the light arrives at.
  ///
  /// Nought is a hard edge that crawls along a curved surface as it turns; a few hundredths is the
  /// width a brush would leave.
  float bandSoftness{.03f};
  /// @brief Where the terminator falls, as a cosine. Above nought pushes the shaded side towards the
  /// light, which is how a drawing makes a form read as rounder than the geometry is.
  float terminator{.05f};
  /// @brief What the shaded side is multiplied by. A cool tint is the drawn convention: a shadow in
  /// a cel is a colour of its own, not the lit colour darkened.
  glm::vec3 shadowTint{.66f, .62f, .86f};
  /// @brief How large the hard highlight is, as a share of the hemisphere. Nought draws none.
  float highlightSize{.06f};
  /// @brief The rim of light along a silhouette, and how far in from the edge it reaches.
  float rimIntensity{.5f};
  float rimWidth{.18f};
  /// @brief How thick the ink is, in pixels of a picture a thousand and eighty high. Nought draws no
  /// lines at all and costs the pass a copy.
  float inkThickness{1.4f};
  /// @brief How far apart in depth two neighbours must be, as a share of the nearer's distance, for
  /// the nearer to be drawn as an outline.
  float inkDepthThreshold{.06f};
  /// @brief How sharply a surface must fold, measured off the depth alone, for the fold to be inked.
  float inkCreaseThreshold{.35f};
  /// @brief The ink, and how much of the colour beneath it the ink takes on. Lines in a cel are
  /// rarely black: a line across a red sleeve is a dark red.
  glm::vec3 inkColor{.04f, .03f, .07f};
  float inkTint{.55f};
  /// @brief Distance at which the lines have faded out, in scene units. A painted backdrop carries
  /// no outline, and the fade is what lets the stage's far end read as one.
  float inkFadeDistance{30.f};
  /// @brief The grade, applied to the finished picture after it has been tone mapped.
  float saturation{1.12f};
  float contrast{1.06f};
  /// @brief What the darks are pushed towards and what the lights are, in display space. Neutral
  /// grey is no push at all.
  glm::vec3 shadowColor{.5f, .5f, .5f};
  glm::vec3 highlightColor{.5f, .5f, .5f};
  /// @brief How dark the corners are drawn, from nought to one.
  float vignette{.22f};
  auto operator==(const Stylization &) const -> bool = default;
};
} // namespace kuki
