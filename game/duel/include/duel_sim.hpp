#pragma once
#include <animator.hpp>
#include <array>
#include <cstdint>
#include <glm/ext/matrix_float3x3.hpp>
#include <gesture.hpp>
#include <rollback.hpp>
#include <span>
#include <string_view>
#include <vector>
namespace kuki {
struct ModelAsset;
} // namespace kuki
namespace duel {
/// A number for this game and its version, given to the network system. Two machines that give
/// different ones are not let play each other, so change it whenever a change to the rules would make
/// an old build and a new one play different games.
inline constexpr uint32_t PROTOCOL = 1;
/// The bit each button has in the mask, in the order the game registers them.
enum Button : int { Left,
  Right,
  Punch,
  Kick };
/// The states a figure can be in, which is also their order in `DuelStates`.
enum State : int { Idle,
  Walk,
  Back,
  Lunge,
  Punching,
  Kicking,
  Hit };
/// @brief Where a figure stands, as far as the rules are concerned.
///
/// Along the floor, which is the X axis; nothing in the game moves on any other. The scene's copy of
/// this is what is drawn, somewhere between `previous` and `position` depending on how far the frame
/// is between two ticks, and a rule that read it would be reading a picture.
struct Fighter {
  float position{};
  float previous{};
  /// +1 facing up the axis, -1 facing down it.
  float facing{1.f};
  int hits{};
};
/// @brief A blow that was thrown, for whatever is drawing to show.
///
/// Not part of the game's state and not rolled back: it is a record kept beside it. A tick that is
/// run again and throws the same blow finds its record by the tick and the one who threw it, and
/// updates it, so that a spark already shown for it is not shown twice.
struct Impact {
  int64_t tick{};
  int attacker{};
  bool hit{};
  /// Where it landed, along the floor and up from it.
  float x{};
  float height{};
  /// Set by whoever shows it, once they have.
  bool shown{};
};
/// @brief What a player does with nobody at the keyboard: walk at the other, stop now and then, and
/// throw blows on ticks far enough apart for each to finish.
///
/// A function of the player and the tick and nothing else, so it can stand in for a person in a test
/// or a second window and be the same every time. Enough to meet, strike, miss and be struck.
auto ScriptedButtons(const int, const int64_t) -> kuki::ButtonMask;
/// @brief The states a figure moves between, in the order of `State`.
auto DuelStates() -> std::vector<kuki::AnimationState>;
/// @brief How a root node's travel is turned into distance along the floor.
///
/// A clip moves the root in the space of whatever the root hangs from, which an importer is free to
/// have scaled or stood on end, and `AnimationSystem::Tick` takes it through that parent into the
/// entity's own axes. What that comes to does not depend on the scene: the entity's own turn cancels,
/// leaving its scale and the nodes between it and the root. So it can be worked out from the model
/// alone, which is what lets the rules run with no scene at all.
auto RootSpace(const kuki::ModelAsset &, const std::string_view) -> glm::mat3;
/// @brief The whole of the game: both figures, what each player's hands have been doing, and what
/// counts as a hit.
///
/// Nothing in here knows about a window, the scene or the application. A figure is a position and an
/// `Animator` stepped by `StepAnimator` on its own, with the model as the only thing it is given, so
/// the game can be run in a test, run twice over to compare, or put back to a tick and run on. That
/// is what a rollback does to it, and why it is a `RollbackGame`.
///
/// `Match` is the other half: it asks this what the buttons did and draws the answer.
class DuelSim final : public kuki::RollbackGame {
public:
  static constexpr int PLAYERS = 2;
  /// @brief A game of two figures on a floor, given the model whose clips decide how long moves take.
  ///
  /// The model must outlive the game. `rootSpace` is from `RootSpace`, and the identity does for a
  /// model that is not scaled or turned.
  DuelSim(const kuki::ModelAsset &, const int tickRate, const glm::mat3 &rootSpace = glm::mat3(1.f));
  /// @brief Runs one tick. `buttons` is indexed by player, and each one is a mask of `Button`s as
  /// they are on the keyboard: left is left along the floor whichever way the figure faces.
  auto Advance(const int64_t, const std::span<const kuki::ButtonMask>, const bool) -> void override;
  auto Save(std::vector<std::byte> &) const -> void override;
  auto Load(const std::span<const std::byte>) -> void override;
  /// @brief What a player who is not there presses: close the distance, and now and then punch.
  ///
  /// A function of the state alone, so that it is the same every time the tick is run.
  auto Think(const int) const -> kuki::ButtonMask;
  auto GetTick() const -> int64_t;
  auto GetRootSpace() const -> const glm::mat3 &;
  auto GetFighter(const int) const -> const Fighter &;
  auto GetAnimator(const int) const -> const kuki::Animator &;
  auto GetImpacts() -> std::vector<Impact> &;
  /// @brief Whether a blow is written to the log when it is thrown. On, except on ticks run again.
  auto SetLogging(const bool) -> void;
private:
  const kuki::ModelAsset &model;
  int tickRate;
  glm::mat3 rootSpace;
  std::array<Fighter, PLAYERS> fighters;
  std::array<kuki::Animator, PLAYERS> animators;
  /// One for each player, local or not: they keep nothing but the masks, so each is a function of
  /// what its player pressed.
  std::array<kuki::GestureRecognizer, PLAYERS> gestures;
  std::vector<Impact> impacts;
  kuki::GestureID punch{kuki::INVALID_GESTURE};
  kuki::GestureID kick{kuki::INVALID_GESTURE};
  kuki::GestureID lunge{kuki::INVALID_GESTURE};
  int64_t tick{};
  bool replaying{};
  bool logging{true};
  /// @brief Moves a figure by what its animation travelled this tick and resolves what it started.
  auto Step(const int) -> void;
  auto Strike(const int, const float, const float) -> void;
  /// @brief Whether a figure is doing nothing it has to finish, and so may start something.
  auto IsFree(const kuki::Animator &) const -> bool;
};
} // namespace duel
