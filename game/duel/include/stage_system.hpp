#pragma once
#include <array>
#include <command_line.hpp>
#include <cstdint>
#include <input_manager.hpp>
#include <stage.hpp>
#include <system.hpp>
namespace kuki {
class Application;
} // namespace kuki
/// @brief The place the match is fought in, and everything in it that moves without being told to.
///
/// Builds the stage the command line names, or the first one, once the game has built its scene;
/// builds the next one when Tab is pressed; and keeps the stage alive in between -- fires that
/// flicker, a beacon that blinks, a sign that stutters, lightning over the alley.
///
/// Presentation only, like the scene it dresses. Nothing here is read by the rules, so two machines
/// playing each other on different stages are playing the same game.
///
/// Listed after `DuelSystem`, which writes the scene's `ScreenEffects` afresh each frame: a flash
/// of lightning is laid over whatever the fight has already put there, rather than written under it
/// and lost.
class StageSystem final : public kuki::System {
public:
  static constexpr kuki::Flag STAGE_FLAG{"--stage", kuki::FlagKind::Value, "<name>", "the stage to fight on: alley, temple, rooftop or shrine"};
  static constexpr std::array FLAGS{STAGE_FLAG};
  explicit StageSystem(kuki::Application &);
  auto PostStart() -> void override;
  auto Update(const float) -> void override;
  auto Shutdown() -> void override;
private:
  duel::StageScene stage;
  kuki::InputManager::ActionID nextAction{kuki::InputManager::InvalidActionID};
  bool switchPending{};
  /// Whether the blinking lights were lit last frame, so their glow goes off once as they come on.
  bool blinkWasOn{};
  /// Seconds since the stage was built, which every flicker is a function of.
  float clock{};
  /// When the next bolt falls, and how far into the one falling now the clock is; below nought
  /// when none is.
  float nextStrike{};
  float strike{-1.f};
  /// The state of a small generator of its own, so the flicker and the lightning need nothing from
  /// the rules' random numbers -- which they must not touch, since a rollback replays them.
  uint32_t seed{0x9e3779b9u};
  auto Random() -> float;
  auto Build(const duel::StageId) -> void;
  auto AnimateLights() -> void;
  auto Storm(const float) -> void;
};
