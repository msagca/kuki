#pragma once
#include <animation_system.hpp>
#include <application_description.hpp>
#include <rendering_system.hpp>
#include <scripting_system.hpp>
#include <system_application.hpp>
inline constexpr const char *DUEL_FONT = "font/Inter-VariableFont_opsz,wght.ttf";
inline constexpr const char *FIGHTER_MODEL = "fighter.glb";
namespace kuki {
class Application;
} // namespace kuki
/// @brief Loads the game's assets and builds its scene into whichever application is hosting it.
///
/// The game's own, below, or the editor. Everything that makes this a game is done here and in the
/// scripts the scene names, so that neither host has anything to do but call it.
auto SetupDuel(kuki::Application &) -> void;
/// @brief Two figures on a strip of floor, one of them played from the keyboard.
///
/// Small on purpose. It is here to put the fixed tick, the gesture recognizer and the animator's
/// states to work together on something with rules, and it has only as much game as that takes.
///
/// Animation is listed ahead of scripting. Both are stepped by the tick, and in this order a script
/// sees the events a tick fired -- the moment a punch lands -- during that same tick rather than
/// the one after.
class Duel final : public kuki::SystemApplication<kuki::AnimationSystem, kuki::ScriptingSystem, kuki::RenderingSystem> {
public:
  Duel();
private:
  auto Start() -> void override;
};
