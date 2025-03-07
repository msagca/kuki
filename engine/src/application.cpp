#include <application.hpp>
#include <chrono>
#include <scene.hpp>
#include <system.hpp>
Application::Application()
  : assetManager(), assetLoader(assetManager) {}
Scene* Application::CreateScene() {
  auto scene = new Scene(assetManager);
  scenes.push_back(scene);
  return scenes.back();
}
void Application::DeleteScene(unsigned int id) {
  if (id >= scenes.size())
    return;
  scenes.erase(scenes.begin() + id);
  if (activeSceneID >= scenes.size())
    activeSceneID = 0;
}
Scene* Application::GetActiveScene() {
  if (scenes.size() == 0)
    return nullptr;
  if (activeSceneID >= scenes.size())
    activeSceneID = 0;
  return scenes[activeSceneID];
}
void Application::SetActiveScene(unsigned int id) {
  if (id < scenes.size())
    activeSceneID = id;
}
void Application::Start() {
  for (auto system : systems)
    system->Start();
};
bool Application::Status() {
  return true;
};
void Application::Update() {
  const auto timeNow = std::chrono::high_resolution_clock::now();
  static auto timeLast = timeNow;
  deltaTime = std::chrono::duration<float>(timeNow - timeLast).count();
  timeLast = timeNow;
  auto activeScene = GetActiveScene();
  if (!activeScene)
    return;
  for (auto system : systems)
    system->Update(deltaTime, activeScene);
};
void Application::Shutdown() {
  activeSceneID = 0;
  scenes.clear();
  for (const auto system : systems)
    system->Shutdown();
  systems.clear();
};
void Application::Run() {
  Start();
  while (Status())
    Update();
  Shutdown();
}
