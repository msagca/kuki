#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace kuki {
/// @brief How a camera decides the scale it applies to scene radiance.
///
/// The order is load-bearing: `EnumTraits<ExposureMode>` names these by index and the scene
/// serialiser writes the index, so inserting one in the middle changes what an existing scene means.
enum class ExposureMode : uint8_t {
  /// @brief A number of stops, set directly.
  ///
  /// No pretence of a physical camera: whatever the light in the scene happens to be, this is the
  /// scale that brings it onto the display. The right choice while the lights carry arbitrary
  /// intensities, because it is the only one that stays honest about being a viewing adjustment.
  Manual,
  /// @brief Derived from an aperture, a shutter time and a sensitivity, the way a camera does it.
  ///
  /// Correct, and currently not useful, which is worth stating plainly. The formula assumes the
  /// scene emits in photometric units, and this engine's lights do not: their `intensity` is a bare
  /// multiplier with a constant, linear and quadratic falloff rather than an emitter measured in
  /// lumens or candela. Every plausible setting therefore lands on a near black image, and reaching
  /// a visible one means a sensitivity no real sensor has.
  ///
  /// It is here so the shape exists and so the numbers can be checked against a light meter later.
  /// It becomes the mode worth using on the day lights are photometric, and nothing about it has to
  /// change on that day.
  Physical
};
/// @brief Stops of exposure compensation a camera starts with.
///
/// Zero means the scene reaches the curve at the radiance the lights emit, which is the only
/// defensible default: any other is a guess about how bright someone's scene is, silently folded
/// into every image until they find the control and take it back out.
inline constexpr float DEFAULT_EXPOSURE = .0f;
/// @brief Range the compensation control offers, in stops either side of zero.
inline constexpr float EXPOSURE_RANGE = 8.f;
inline constexpr float DEFAULT_APERTURE = 2.8f;
inline constexpr float DEFAULT_SHUTTER_SPEED = 1.f / 125.f;
inline constexpr float DEFAULT_SENSITIVITY = 400.f;
/// @brief Bounds on the physical controls, chosen to cover real equipment and no more.
///
/// These are clamps rather than suggestions, and the reason is the arithmetic rather than realism:
/// `ComputeEV100` divides by the shutter time and by the sensitivity, so a zero reaching either
/// produces an infinity that becomes a black or white frame with nothing on screen to explain it.
inline constexpr float MIN_APERTURE = .7f;
inline constexpr float MAX_APERTURE = 32.f;
inline constexpr float MIN_SHUTTER_SPEED = 1.f / 8000.f;
inline constexpr float MAX_SHUTTER_SPEED = 30.f;
inline constexpr float MIN_SENSITIVITY = 25.f;
inline constexpr float MAX_SENSITIVITY = 204800.f;
/// @brief Calibration relating a meter's reading to the luminance that saturates the sensor.
///
/// The constant every physically based exposure implementation carries. It is a property of how
/// meters are calibrated rather than anything derivable, and 1.2 is the value in common use.
inline constexpr float INCIDENT_METER_CALIBRATION = 1.2f;
/// @brief Exposure value at ISO 100 for an aperture, shutter time and sensitivity.
///
/// `EV100 = log2(N^2 / t) - log2(S / 100)`, the standard definition. Larger means more light is
/// being kept out, so a brighter scene wants a larger number.
inline auto ComputeEV100(const float aperture, const float shutterSpeed, const float sensitivity) -> float {
  const auto n = std::clamp(aperture, MIN_APERTURE, MAX_APERTURE);
  const auto t = std::clamp(shutterSpeed, MIN_SHUTTER_SPEED, MAX_SHUTTER_SPEED);
  const auto s = std::clamp(sensitivity, MIN_SENSITIVITY, MAX_SENSITIVITY);
  return std::log2(n * n / t * 100.f / s);
}
/// @brief Stops of scale to apply to scene radiance, for either kind of exposure.
///
/// Both modes end in the same currency, a count of stops, so that everything downstream stays one
/// multiply and neither backend has to know which mode produced the number. A physical setup
/// saturates at a luminance of `1.2 * 2^EV100` and so scales by the reciprocal of it, which is
/// `-EV100 - log2(1.2)` once written in stops; compensation then adds to whichever mode is in use,
/// so it reads the same way in both.
inline auto ComputeExposureStops(const ExposureMode mode, const float compensation, const float aperture, const float shutterSpeed, const float sensitivity) -> float {
  if (mode == ExposureMode::Manual)
    return compensation;
  return -ComputeEV100(aperture, shutterSpeed, sensitivity) - std::log2(INCIDENT_METER_CALIBRATION) + compensation;
}
} // namespace kuki
