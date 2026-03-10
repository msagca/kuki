#include <game.hpp>
Game::Game()
  : Application() {}
Game::~Game() {
  Shutdown();
}
void Game::Start() {}
auto Game::Status() -> bool {
  return true;
}
auto Game::Update() -> void {}
auto Game::LateUpdate() -> void {}
auto Game::Shutdown() -> void {}
