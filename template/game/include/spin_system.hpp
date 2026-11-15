#pragma once
#include <system.hpp>
namespace kuki {
class Application;
} // namespace kuki
namespace my_game {
/// @brief The binding that turns every spinner the other way.
inline constexpr const char *REVERSE = "MyGame.Reverse";
/// @brief Turns every `Spin` entity, a tick at a time.
///
/// In `Tick` rather than `Update`, because it is a rule: it changes the state of the game, so it
/// runs at the fixed rate and gives the same result on every machine.
class SpinSystem final : public kuki::System {
public:
  explicit SpinSystem(kuki::Application &);
  auto Update(const float) -> void override;
  auto Tick() -> void override;
private:
  float direction{1.f};
};
} // namespace my_game
