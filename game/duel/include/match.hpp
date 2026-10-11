#pragma once
#include <array>
#include <gesture.hpp>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <input_manager.hpp>
#include <script.hpp>
#include <string>
namespace kuki {
struct Animator;
} // namespace kuki
/// @brief One of the two figures, as far as the rules are concerned.
///
/// Where it stands is kept here and not read back from the scene: the scene's copy is what is
/// drawn, somewhere between `previous` and `position` depending on how far the frame is between
/// two ticks, and a rule that read it would be reading a picture.
struct Fighter {
  kuki::EntityID entity{};
  /// Along the floor. The floor is the X axis; nothing in the game moves on any other.
  float position{};
  float previous{};
  /// +1 facing up the axis, -1 facing down it.
  float facing{1.f};
  int hits{};
};
/// @brief The whole of the game: both figures, the player's hands, and what counts as a hit.
///
/// Everything that decides anything is in `Tick`. `Update` only draws -- it puts the figures
/// between their last two places and writes the text.
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
  enum Button : int { Left,
    Right,
    Punch,
    Kick };
  std::array<Fighter, 2> fighters;
  kuki::GestureRecognizer gestures;
  kuki::GestureID punch{kuki::INVALID_GESTURE};
  kuki::GestureID kick{kuki::INVALID_GESTURE};
  kuki::GestureID lunge{kuki::INVALID_GESTURE};
  kuki::EntityID sparks{};
  kuki::InputManager::ActionID quitAction{kuki::InputManager::InvalidActionID};
  auto Spawn(kuki::Application &, const int, const float, const float) -> void;
  auto GetAnimator(kuki::Application &, const int) -> kuki::Animator *;
  /// @brief Whether a figure is doing nothing it has to finish, and so may start something.
  auto IsFree(const kuki::Animator &) const -> bool;
  /// @brief Moves a figure by what its animation travelled this tick and resolves what it started.
  auto Step(kuki::Application &, const int) -> void;
  auto Strike(kuki::Application &, const int, const float, const float) -> void;
};
