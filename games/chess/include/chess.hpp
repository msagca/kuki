#pragma once
#include <application_description.hpp>
#include <chess_world.hpp>
#include <game_manager.hpp>
#include <rendering_system.hpp>
#include <stress_system.hpp>
#include <system_application.hpp>
namespace kuki {
class Application;
} // namespace kuki
namespace chess {
/// @brief Loads the game's assets and builds its scene into the application it is given.
auto SetupChess(kuki::Application &) -> void;
/// @brief The typeface the game sets all of its text in, staged beside the binary.
///
/// One font for the clocks and the board's labels alike. `Application::SetOverlayFont` bakes it
/// and registers the atlas, and `GameManager` reads that same atlas back rather than baking its
/// own -- a second bake of the same file would be a second four-megabyte texture saying exactly
/// what the first one says.
inline constexpr const char *CHESS_FONT = "fonts/Inter-Regular.ttf";
/// @brief The chess application: a window, a renderer, the game, and nothing else.
///
/// `SystemApplication` takes the systems as template parameters, so the ones a game does not use
/// cost it nothing. There is no `AnimationSystem` and no `PhysicsSystem` here: chess has neither
/// skinned meshes nor anything that falls. `RenderingSystem` is the one that has to be present -- `Application`'s own startup
/// depends on it, and leaving it out is a `static_assert` rather than a crash.
class Chess final : public kuki::SystemApplication<ChessWorld, GameManager, kuki::StressSystem, kuki::RenderingSystem> {
public:
  Chess();
private:
  auto Start() -> void override;
};
} // namespace chess
