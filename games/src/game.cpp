#include <game.hpp>
Game::Game()
  : Application(AppConfig{}) {}
Game::~Game() {
  Shutdown();
}
void Game::Start() {}
bool Game::Status() {
  return true;
}
void Game::Update() {}
void Game::LateUpdate() {}
void Game::Shutdown() {}
