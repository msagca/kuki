#pragma once
#include <my_game_world.hpp>
#include <rendering_system.hpp>
#include <spin_system.hpp>
#include <system_application.hpp>
namespace kuki {
class Application;
} // namespace kuki
/// @brief Registers the controls, makes the assets and builds the scene.
auto SetupMyGame(kuki::Application &) -> void;
/// @brief The application: the game's world, and its systems in the order they run.
/// `RenderingSystem` is required, and last.
class MyGame final : public kuki::SystemApplication<my_game::MyGameWorld, my_game::SpinSystem, kuki::RenderingSystem> {
public:
  MyGame();
private:
  auto Start() -> void override;
};
