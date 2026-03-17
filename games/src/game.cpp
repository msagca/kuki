#include <game.hpp>
Game::Game()
  : Application() {}
Game::~Game() {
  Shutdown();
}
auto Game::Awake() -> void {}
void Game::Start() {}
auto Game::Update(const float) -> void {}
auto Game::Shutdown() -> void {}
auto Game::Status() -> bool {
  return true;
}
