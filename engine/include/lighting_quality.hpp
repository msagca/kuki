#pragma once
#include <cstdint>
namespace kuki {
/// @brief How much of the indirect lighting a machine can afford, chosen in the settings menu.
///
/// The light that bounces off surfaces comes from the probe field, which is traced a budget of
/// probes a frame (`PROBE_TRACE_BUDGET`) and was the largest single cost in the frontier's frame.
/// The sky's own light and the sun do not depend on it, so every level keeps those.
///
/// - `Low` stands the trace down and draws no bounce at all: surfaces are lit by the sun and the sky
///   alone. Standing the trace down by itself would not be enough -- a bypassed trace leaves the
///   field as it was, still lit, and still sampled -- so the bounce is also scaled to nothing.
/// - `Medium` traces half the budget a frame. A converged field looks the same; a change in the light
///   takes twice as long to reach every probe, which on the frontier is two thirds of a second.
/// - `High` is the budget as measured, and what a scene was made to be seen with.
enum class LightingQuality : uint8_t {
  Low,
  Medium,
  High
};
} // namespace kuki
