#pragma once
#include <array>
#include <glm/ext/vector_float3.hpp>
#include <command_line.hpp>
#include <duel_sim.hpp>
#include <duel_world.hpp>
#include <id.hpp>
#include <input_manager.hpp>
#include <memory>
#include <string>
#include <system.hpp>
namespace kuki {
class Application;
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
///
/// It was a script on an entity of its own, and is a system because nothing about it is one
/// entity's: it is the whole match. The figures are found each time by the `Seat` they were made
/// with, through `DuelWorld::ForEach`, which the compiler narrows to the one archetype that has one.
class DuelSystem final : public kuki::System {
public:
  /// @brief Leaves the figures as their clips pose them. See `PostStart`.
  static constexpr kuki::Flag NO_IK_FLAG{"--no-ik", kuki::FlagKind::Switch, "", "leave the figures as their clips pose them, without turning their heads to each other"};
  /// @brief Keeps blows from flashing the whole screen. See `ShowImpact`.
  static constexpr kuki::Flag NO_FLASH_FLAG{"--no-flash", kuki::FlagKind::Switch, "", "show blows without the impact frames and flashes, for anyone the flashing troubles"};
  static constexpr std::array FLAGS{NO_IK_FLAG, NO_FLASH_FLAG};
  explicit DuelSystem(kuki::Application &);
  /// @brief Registers the buttons, places the figures and hands the rules to the network system.
  ///
  /// After the game has built its scene, which is where the fighter's model was loaded.
  auto PostStart() -> void override;
  auto Tick() -> void override;
  auto Update(const float) -> void override;
  auto Shutdown() -> void override;
private:
  /// Shared and not unique because the network system holds the rules too, for as long as the match.
  std::shared_ptr<duel::DuelSim> sim;
  kuki::EntityID sparks{};
  /// A ring thrown out from where a blow lands, and the dust a lunge or a knock kicks up.
  kuki::EntityID shockwave{};
  kuki::EntityID dust{};
  /// The last blow to land, for as long as the picture is still showing it: how long ago, how hard
  /// -- a kick harder than a punch -- and where. Below nought when none is.
  float hitAge{-1.f};
  float hitStrength{};
  glm::vec3 hitPoint{};
  /// Each figure's state as last drawn, to see a lunge or a knock begin.
  std::array<int, duel::DuelSim::PLAYERS> shownState{};
  /// Where the camera is looking and how far back it stands, eased towards where the fight wants it.
  glm::vec3 cameraFocus{.0f, 1.f, .0f};
  float cameraDistance{6.5f};
  bool cameraPlaced{};
  float clock{};
  /// Whether a blow may invert or whiten the whole screen. Off with `--no-flash`.
  bool flashing{true};
  /// The health each bar shows, and the pale trail that follows it down, as a share of a bar.
  std::array<float, duel::DuelSim::PLAYERS> shownHealth{1.f, 1.f};
  std::array<float, duel::DuelSim::PLAYERS> trailHealth{1.f, 1.f};
  kuki::InputManager::ActionID quitAction{kuki::InputManager::InvalidActionID};
  int frames{};
  auto Spawn(const int) -> kuki::EntityID;
  /// @brief Calls `func(player, id, animator, transform)` for each figure in the scene.
  auto ForEachFigure(auto &&) -> void;
  auto GetAnimator(const int) -> kuki::Animator *;
  /// @brief The entity standing for one of a figure's bones, by the bone's name.
  auto FindBone(const int, const std::string &) -> kuki::EntityID;
  /// @brief Puts the scene's animators in the state the rules have them in.
  auto SyncAnimators() -> void;
  /// @brief Compares the rules' idea of how the root's travel becomes distance with the engine's.
  ///
  /// The rules work it out from the model so that they need no scene; the animation system works it
  /// out from the scene's transforms. They are meant to come to the same thing, and if an asset is
  /// ever exported in a way that makes them differ, the figures would walk at the wrong speed with
  /// nothing to say why.
  auto CheckRootSpace() -> void;
  /// @brief Follows the fight: centred between the figures, further back the further apart they
  /// are, shaken for a moment by a blow.
  auto MoveCamera(const float) -> void;
  /// @brief What a blow does to the whole picture for the moments after it lands.
  auto ShowImpact(const float) -> void;
  /// @brief The bars along the top, the names under them and the keys down the side.
  auto DrawHud() -> void;
};
