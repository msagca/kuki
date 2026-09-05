#pragma once
#include <cstdint>
#include <tone_mapper.hpp>
namespace kuki {
/// @brief Luminance a texel has to exceed before it contributes to bloom.
///
/// This and the values below are shared by both backends rather than written into each one. The
/// post-processing shaders are ports of one another, so a constant that lives in only one of them
/// is a constant the two will eventually disagree on, and the disagreement shows up as a scene
/// that glows differently depending on which backend drew it.
inline constexpr float BRIGHT_PASS_THRESHOLD = .5f;
/// @brief How much of the blurred bright pass is added back over the scene.
inline constexpr float BLOOM_INTENSITY = .5f;
/// @brief Exponent the tone mapped image is raised to the reciprocal of, for display.
///
/// Ignored under `ToneMapper::AgX`, whose curve lands in display space on its own; every other
/// operator ends in radiance and needs this to get there.
inline constexpr float GAMMA = 2.2f;
/// @brief Curve the tone mapping pass starts out using, before a config or the editor says otherwise.
///
/// The curve lives here, and the exposure that feeds it does not: see `Camera::exposureMode`. One
/// describes the display being worked at and belongs to the installation, the other describes the
/// camera and belongs to the scene.
inline constexpr ToneMapper DEFAULT_TONE_MAPPER = ToneMapper::AgX;
/// @brief Number of blur passes the bloom chain runs over the bright pass.
///
/// Each pass filters a single axis and they alternate, so the count is twice the number of full
/// two-dimensional blurs. Width is bought with passes rather than with a wider kernel because
/// repeating a small Gaussian converges on a much wider one for far fewer texture fetches. The
/// count has to be even, or the chain ends on a horizontal pass and the result is blurred further
/// across one axis than the other.
inline constexpr uint32_t BLUR_PASS_COUNT = 8;
} // namespace kuki
