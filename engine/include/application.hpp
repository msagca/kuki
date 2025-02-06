#pragma once
#include <engine_export.h>
#include <scene.hpp>
#include <system.hpp>
#include <vector>
class ENGINE_API Application {
protected:
  float deltaTime = .0f;
  std::vector<Scene*> scenes;
  std::vector<System*> systems;
  unsigned int activeSceneID = 0;
  AssetManager assetManager;
  AssetLoader assetLoader;
public:
  Application();
  virtual ~Application() = default;
  virtual void Start();
  virtual bool Status();
  virtual void Update();
  virtual void Shutdown();
  void Run();
  Scene* CreateScene();
  void DeleteScene(unsigned int);
  Scene* GetActiveScene();
  void SetActiveScene(unsigned int);
  template <typename T, typename... Z>
  T* CreateSystem(Z&&... args);
  template <typename T>
  void DeleteSystem();
  template <typename T>
  T* GetSystem();
};
template <typename T, typename... Z>
T* Application::CreateSystem(Z&&... args) {
  static_assert(std::is_base_of_v<System, T>, "T must inherit from System.");
  auto system = new T(std::forward<Z>(args)...);
  systems.push_back(dynamic_cast<System*>(system));
  return system;
}
template <typename T>
void Application::DeleteSystem() {
  auto it = std::remove_if(systems.begin(), systems.end(), [](System* system) {
    auto casted = dynamic_cast<T*>(system);
    if (casted) {
      delete casted;
      return true;
    }
    return false;
  });
  systems.erase(it, systems.end());
}
template <typename T>
T* Application::GetSystem() {
  for (auto system : systems) {
    auto casted = dynamic_cast<T*>(system);
    if (casted)
      return casted;
  }
  return nullptr;
}
