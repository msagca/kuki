#pragma once
#include <system.hpp>
namespace kuki {
class Application;
} // namespace kuki
/// @brief The weather and the hour: the wind that moves everything, the foam breathing on the shore,
/// and the sun.
///
/// The wind turns slowly and rises and falls, by two slow noises of the clock, and is written to
/// the scene's `Wind` -- which bends the grass and the trees and raises the waves -- and to
/// whatever the wind should agree with: the clouds drift its way, and every emitter whose particles
/// wander (`turbulence` above nought: leaves, pollen, spray, smoke) is carried along it.
///
/// T lets the day go by -- the sun climbing from dawn to noon and back down to dusk, and the light
/// warming and cooling with it -- and T again holds it where it is.
class AmbienceSystem final : public kuki::System {
public:
  explicit AmbienceSystem(kuki::Application &);
  auto Update(const float) -> void override;
private:
  float clock{};
  /// How far through the day it is, from dawn at nought to dusk at one, and whether it is passing.
  float day{.28f};
  bool passing{};
  float dayDirection{1.f};
  auto Weather() -> void;
  auto Sun() -> void;
};
