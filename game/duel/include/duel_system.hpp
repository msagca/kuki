#pragma once
#include <array>
#include <command_line.hpp>
#include <cstddef>
#include <cstdint>
#include <duel_sim.hpp>
#include <duel_world.hpp>
#include <gesture.hpp>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <input_manager.hpp>
#include <memory>
#include <roster.hpp>
#include <string>
#include <system.hpp>
#include <vector>
namespace kuki {
class Application;
struct Animator;
} // namespace kuki
/// @brief The scene's half of the game: the screen a fighter is picked on, and the match after it.
///
/// The game opens on the roster standing in a row. A and D move along it and Enter starts a match
/// with the one chosen against another. Escape is the settings menu's, as in every game: leaving a
/// match for the row is one of the entries on its game page, and quitting is the menu's own.
///
/// In a match it feeds the rules the buttons and draws what they decide. The rules are in
/// `duel::DuelSim`, which knows nothing of the scene. That is deliberate and it is what makes the
/// game something two machines can play: the rules can be taken back to an earlier tick and run
/// again, and a scene cannot. So the scene is only ever a picture of the rules. Each tick this hands
/// them the buttons and copies the answer into the scene's animators; each frame it puts the figures
/// where the rules have them, between the last two ticks.
///
/// The animators in the scene are therefore not what decides anything. They are stepped by
/// `AnimationSystem` as they always were, and then set to what the rules say, which is the same
/// except on the tick the rules disagree -- a blow landing, or a tick being corrected.
///
/// It was a script on an entity of its own, and is a system because nothing about it is one
/// entity's: it is the whole match. The figures are found each time by the `Seat` they were made
/// with, through `DuelWorld::ForEach`, which the compiler narrows to the one archetype that has one.
/// The roster on the select screen has seats too, numbered below nought, so it is never mistaken for
/// a player.
class DuelSystem final : public kuki::System {
public:
  /// @brief Leaves the figures as their clips pose them. See `StartFight`.
  static constexpr kuki::Flag NO_IK_FLAG{"--no-ik", kuki::FlagKind::Switch, "", "leave the figures as their clips pose them, without turning their heads to each other"};
  /// @brief Keeps blows from flashing the whole screen. See `ShowImpact`.
  static constexpr kuki::Flag NO_FLASH_FLAG{"--no-flash", kuki::FlagKind::Switch, "", "show blows without the impact frames and flashes, for anyone the flashing troubles"};
  static constexpr std::array FLAGS{NO_IK_FLAG, NO_FLASH_FLAG};
  /// @brief The binding that starts a match from the select screen, Enter unless rebound.
  static constexpr const char *START_BINDING = "Duel.Start";
  /// @brief Keys the two presentation settings are kept under. See `kuki::Preferences`.
  static constexpr const char *PREF_WATCH = "duel.watch";
  static constexpr const char *PREF_FLASH = "duel.flash";
  explicit DuelSystem(kuki::Application &);
  /// @brief Whether a blow, or the stage's lightning, may whiten or invert the whole screen.
  auto IsFlashing() const -> bool {
    return flashing;
  }
  /// @brief Registers the buttons and the keys, makes the effects, lists its settings, and opens the
  /// select screen.
  ///
  /// After the game has built its scene, which is where the roster's models were loaded.
  auto PostStart() -> void override;
  auto Tick() -> void override;
  auto Update(const float) -> void override;
  auto Shutdown() -> void override;
private:
  enum class Screen : uint8_t {
    Select,
    Fight
  };
  Screen screen{Screen::Select};
  /// Which fighter each player plays, by place in the roster: the one the player chose, and the one
  /// the game set against them.
  std::array<size_t, duel::DuelSim::PLAYERS> picks{0, 1};
  /// The roster standing in a row while one is chosen, and how far forward each has stepped.
  std::vector<kuki::EntityID> lineup;
  std::array<float, duel::ROSTER.size()> stepped{};
  /// The light that picks out the one chosen.
  kuki::EntityID spotlight{};
  /// The buttons as they stood on the last tick, to see one go down.
  kuki::ButtonMask previousButtons{};
  /// Asked for by the settings menu and carried out in `Update`, at the top of the next frame rather
  /// than between the systems' updates and the drawing of this one.
  bool backPending{};
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
  /// Where the camera is looking and how far back it stands, eased towards where the screen wants it.
  glm::vec3 cameraFocus{.0f, 1.f, .0f};
  float cameraDistance{6.5f};
  bool cameraPlaced{};
  float clock{};
  /// Whether the figures turn their heads to each other, and whether a blow may invert or whiten the
  /// whole screen. Both are settings, kept between runs; `--no-ik` and `--no-flash` turn them off
  /// for one run whatever was saved.
  bool watching{true};
  bool flashing{true};
  /// The health each bar shows, and the pale trail that follows it down, as a share of a bar.
  std::array<float, duel::DuelSim::PLAYERS> shownHealth{1.f, 1.f};
  std::array<float, duel::DuelSim::PLAYERS> trailHealth{1.f, 1.f};
  /// A small generator of its own for choosing the opponent, which is presentation and must not
  /// touch anything the rules draw on.
  uint32_t seed{0x2545f491u};
  int frames{};
  /// @brief Places a fighter of the roster: for a player, in a seat of nought or one, or for the row
  /// on the select screen, in a seat below nought.
  auto Spawn(const int seat, const size_t character, const bool alternate = false) -> kuki::EntityID;
  /// @brief Takes every figure out of the scene, players and row alike.
  auto ClearFigures() -> void;
  /// @brief Ends any match and stands the roster in a row to be chosen from.
  auto EnterSelect() -> void;
  /// @brief Sets the one chosen against another and hands the rules to the network system.
  auto StartFight() -> void;
  /// @brief The row: the one chosen steps forward under the light and the camera leans towards it.
  auto UpdateSelect(const float) -> void;
  /// @brief The title and the names under the row.
  auto DrawSelect() -> void;
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
  /// @brief Eases the camera to look at a point from a distance, shaken for a moment by a blow.
  auto AimCamera(const glm::vec3 &, const float, const float) -> void;
  /// @brief Follows the fight: centred between the figures, further back the further apart they are.
  auto MoveCamera(const float) -> void;
  /// @brief What a blow does to the whole picture for the moments after it lands.
  auto ShowImpact(const float) -> void;
  /// @brief The bars along the top and the names under them.
  auto DrawHud() -> void;
  /// @brief Gives each figure in a match the goal that turns its head to the other, or takes it away,
  /// as `watching` says. So the setting takes in the middle of a match and not only at the next.
  auto ApplyWatching() -> void;
  /// @brief Lists the game's settings: leaving the match, and the two about how it is shown.
  auto AddSettings() -> void;
};
