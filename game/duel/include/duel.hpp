#pragma once
#include <animation_system.hpp>
#include <application_description.hpp>
#include <duel_system.hpp>
#include <duel_world.hpp>
#include <network_system.hpp>
#include <rendering_system.hpp>
#include <stage_system.hpp>
#include <system_application.hpp>
inline constexpr const char *DUEL_FONT = "font/Inter-VariableFont_opsz,wght.ttf";
namespace kuki {
class Application;
} // namespace kuki
/// @brief Loads the game's assets and builds its scene into whichever application is hosting it.
///
/// The game's own, below, or the editor. The scene is all it builds: what plays the game is
/// `DuelSystem`, which a host that wants the game played lists among its systems.
auto SetupDuel(kuki::Application &) -> void;
/// @brief A fighter picked from a roster, and a match against another, one of them played from the
/// keyboard.
///
/// Small on purpose. It is here to put the fixed tick, the gesture recognizer and the animator's
/// states to work together on something with rules, and it has only as much game as that takes.
///
/// The network system plays the rules, alone or against another machine, so it is listed ahead of
/// the duel's own system: that draws the game, and finds the rules where this tick left them. The
/// scene's animators are listed ahead of both only because they always were; the rules step their
/// own. The stage comes after the fight, so its lightning is laid over the fight's own flashes.
class Duel final : public kuki::SystemApplication<duel::DuelWorld, kuki::AnimationSystem, kuki::NetworkSystem, DuelSystem, StageSystem, kuki::RenderingSystem> {
public:
  Duel();
private:
  auto Start() -> void override;
};
