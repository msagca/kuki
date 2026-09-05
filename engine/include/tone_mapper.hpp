#pragma once
#include <cstdint>
namespace kuki {
/// @brief Curve the tone mapping pass fits the scene's unbounded radiance onto a display with.
///
/// A renderer producing physically meaningful light has no upper bound on what a pixel can carry,
/// and a display does. Something has to decide what becomes of everything above white, and that
/// decision is most of what separates an image reading as a photograph from one reading as a
/// render. It is a look rather than a correctness setting, which is why it is offered rather than
/// fixed.
///
/// The order is load-bearing twice over: `EnumTraits<ToneMapper>` names these by index, and both
/// backends' shaders branch on the same numbering, so inserting one in the middle relabels every
/// entry after it and silently changes which curve a saved config selects.
enum class ToneMapper : uint8_t {
  /// @brief No curve. Anything above white is clipped, one channel at a time.
  ///
  /// A reference rather than a look. Clipping channels independently drags every over-bright colour
  /// towards whichever primaries survive longest, so a saturated red clips to red and then to white
  /// through a hue that was never in the scene. That artefact is the thing every other entry here
  /// exists to avoid, which makes this the honest comparison when judging whether a curve is
  /// earning its cost.
  None,
  /// @brief `c / (c + 1)`, per channel.
  ///
  /// What this engine used to do, kept for comparison. It never clips, so it is well behaved at the
  /// top end, but applying it per channel compresses the brightest channel hardest and the others
  /// less, which desaturates and shifts hue in step with luminance.
  Reinhard,
  /// @brief Narkowicz's fit to the ACES reference curve.
  ///
  /// The film and games default, and what most reference material and artist expectation is
  /// calibrated against. Contrasty and punchy. Its known failure is that bright saturated colours
  /// drift towards yellow, which shows up most on strong emissives.
  ///
  /// The fit also runs brighter than the reference transform it approximates, which is usually
  /// compensated by scaling its input by around 0.6, a little over half a stop. That is left to the
  /// exposure control rather than folded in here, so that what this applies stays the published
  /// curve and the correction stays visible and adjustable. Expect it to sit brighter than `AgX` at
  /// the same setting: middle grey leaves ACES at about 0.55 and AgX at about 0.50.
  ACES,
  /// @brief Sobotka's AgX, through the polynomial approximation of its contrast curve.
  ///
  /// Compresses towards white along a path that holds hue steady, so a saturated colour desaturates
  /// as it brightens instead of rotating. That makes it the forgiving choice for scenes carrying
  /// strong coloured light and bounce, which is where ACES is at its least convincing.
  ///
  /// This is AgX proper with no look transform layered on top, which is the same neutral form other
  /// renderers ship. Blender's default applies a contrast look over it and so reads punchier than
  /// this does; that look is a separate stage and could be added later without disturbing this one.
  ///
  /// Unlike the others it bakes the display transfer curve into its own output, so the shared gamma
  /// step is skipped for it. See the tone mapping shaders.
  AgX
};
} // namespace kuki
