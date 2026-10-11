#pragma once
#include <algorithm>
#include <cstdint>
namespace kuki {
/// @brief Turns frame time, which is whatever it happens to be, into a whole number of equal ticks.
///
/// A frame is as long as the display and the load make it, and anything advanced by that much is
/// advanced by a different amount on every machine and on every run. That is fine for a camera and
/// wrong for a game's rules: a jump integrated at 144 Hz does not land where the same jump lands at
/// 50, and two machines that are meant to agree about a match cannot. So the rules are stepped by a
/// fixed amount instead, as many times a frame as the time that has gone by calls for -- none, on a
/// frame shorter than a tick, and several after a slow one.
///
/// Nothing here knows about the application. It is arithmetic on a number of seconds, kept apart
/// so that it can be tested without a window.
class TickClock {
public:
  static constexpr int DEFAULT_RATE = 60;
  /// @brief The most ticks one frame is allowed to owe.
  ///
  /// A frame that took a second -- a breakpoint, a level loading, the window being dragged -- owes
  /// sixty ticks, and running them takes long enough that the next frame owes more: the loop falls
  /// behind for good. Past this many the remainder is forgotten rather than repaid, which is to say
  /// the game runs slow for a moment instead of fast for a long one.
  static constexpr int MAX_TICKS_PER_FRAME = 8;
  /// @brief Adds the time a frame took and answers how many ticks are now due.
  ///
  /// The caller runs that many and calls `Complete` after each, so that `Index` names the tick being
  /// run for as long as it is running.
  auto Advance(const double elapsed) -> int {
    if (elapsed > 0.)
      accumulated += elapsed;
    const auto step = Step();
    auto due = 0;
    for (; accumulated >= step && due < MAX_TICKS_PER_FRAME; ++due)
      accumulated -= step;
    if (accumulated >= step)
      accumulated = 0.;
    return due;
  }
  auto Complete() -> void {
    ++index;
  }
  /// @brief How far the present moment is into the tick that has not run yet, from 0 up to 1.
  ///
  /// What drawing wants and the rules must not read. The state a tick leaves is up to a whole tick
  /// stale by the time it is drawn, and by a different fraction every frame, which shows as judder
  /// wherever the display is not an exact multiple of the tick rate. Blending the last two states by
  /// this much removes it.
  auto Alpha() const -> float {
    return static_cast<float>(accumulated / Step());
  }
  /// @brief The zero-based number of the tick being run, or about to be.
  auto Index() const -> uint64_t {
    return index;
  }
  auto Rate() const -> int {
    return rate;
  }
  /// @brief Changes how many ticks make a second. Anything counted in ticks changes length with it.
  auto SetRate(const int value) -> void {
    rate = std::max(1, value);
  }
  /// @brief The length of one tick in seconds.
  auto Step() const -> double {
    return 1. / rate;
  }
private:
  int rate{DEFAULT_RATE};
  double accumulated{};
  uint64_t index{};
};
} // namespace kuki
