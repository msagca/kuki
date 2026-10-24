#pragma once
#include <array>
#include <duel_sim.hpp>
#include <id.hpp>
#include <input_manager.hpp>
#include <memory>
#include <script.hpp>
#include <string>
namespace kuki {
struct Animator;
} // namespace kuki
/// @brief The scene's half of the game: it feeds the rules the buttons and draws what they decide.
///
/// The rules are in `duel::DuelSim`, which knows nothing of the scene. That is deliberate and it is
/// what makes the game something two machines can play: the rules can be taken back to an earlier
/// tick and run again, and a scene cannot. So the scene is only ever a picture of the rules. Each
/// tick this hands them the buttons and copies the answer into the scene's animators; each frame it
/// puts the figures where the rules have them, between the last two ticks.
///
/// The animators in the scene are therefore not what decides anything. They are stepped by
/// `AnimationSystem` as they always were, and then set to what the rules say, which is the same
/// except on the tick the rules disagree -- a blow landing, or a tick being corrected.
class Match final : public kuki::Script {
public:
  Match();
  auto CloneTo(kuki::EntityManager &, const kuki::EntityID) const -> void override;
  auto GetTypeName() const -> std::string override;
  auto Start(kuki::Application &) -> void override;
  auto Tick(kuki::Application &) -> void override;
  auto Update(kuki::Application &) -> void override;
  auto Shutdown(kuki::Application &) -> void override;
private:
  /// Shared and not unique so that a script stays copyable like any other; there is only ever one.
  std::shared_ptr<duel::DuelSim> sim;
  std::array<kuki::EntityID, duel::DuelSim::PLAYERS> entities{};
  kuki::EntityID sparks{};
  kuki::InputManager::ActionID quitAction{kuki::InputManager::InvalidActionID};
  int frames{};
  auto Spawn(kuki::Application &, const int) -> kuki::EntityID;
  auto GetAnimator(kuki::Application &, const int) -> kuki::Animator *;
  /// @brief The entity standing for one of a figure's bones, by the bone's name.
  auto FindBone(kuki::Application &, const int, const std::string &) -> kuki::EntityID;
  /// @brief Puts the scene's animators in the state the rules have them in.
  auto SyncAnimators(kuki::Application &) -> void;
  /// @brief Compares the rules' idea of how the root's travel becomes distance with the engine's.
  ///
  /// The rules work it out from the model so that they need no scene; the animation system works it
  /// out from the scene's transforms. They are meant to come to the same thing, and if an asset is
  /// ever exported in a way that makes them differ, the figures would walk at the wrong speed with
  /// nothing to say why.
  auto CheckRootSpace(kuki::Application &) -> void;
};
