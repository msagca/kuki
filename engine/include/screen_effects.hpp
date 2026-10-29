#pragma once
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
namespace kuki {
/// @brief What is being done to the whole picture this instant: a flash, an impact frame, speed
/// lines. Written by a game every frame and read by the renderer once.
///
/// The renderer reads this and never writes it, as it does an emitter, so nothing here decays on its
/// own. A game that wants a flash to fade fades it, which keeps the shape of the fade -- how long, how
/// sharp -- the game's to choose, and keeps the renderer free of a clock that a paused game would
/// have to stop.
///
/// A scene singleton, and deliberately not saved with the scene: a flash caught by the save would
/// come back as a scene that opens white. What the scene always looks like is `Stylization`.
struct ScreenEffects {
  /// @brief A flash of `flashColor` over everything, from nought to one.
  float flash{};
  glm::vec3 flashColor{1.f, 1.f, 1.f};
  /// @brief The impact frame: the picture thrown to two tones and inverted, the frame a cel animator
  /// cuts in on the instant a blow lands. From nought to one, and meant to last a frame or three.
  float impact{};
  /// @brief Lines streaming in from the edges towards `focus`, from nought to one.
  float speedLines{};
  /// @brief The picture smeared away from `focus`, as a lens racked in an instant. Nought to one.
  float radialBlur{};
  /// @brief The colour channels pulled apart from the centre outwards, in pixels at the corners of
  /// a picture a thousand and eighty high.
  float aberration{};
  /// @brief Where on the picture the lines and the smear centre on, from nought to one across and
  /// down.
  glm::vec2 focus{.5f, .5f};
  auto operator==(const ScreenEffects &) const -> bool = default;
};
} // namespace kuki
