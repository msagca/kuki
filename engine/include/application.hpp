#pragma once
#include <asset_loader.hpp>
#include <engine_export.h>
#include <input_manager.hpp>
#include <scene.hpp>
#include <scene_manager.hpp>
#include <system.hpp>
#include <vector>
class ENGINE_API Application {
private:
  std::string name;
  std::vector<System*> systems;
  void Init();
  static void WindowCloseCallback(GLFWwindow*);
  static void FramebufferSizeCallback(GLFWwindow*, int, int);
  static void CursorPosCallback(GLFWwindow*, double, double);
  static void KeyCallback(GLFWwindow*, int, int, int, int);
  static void MouseButtonCallback(GLFWwindow*, int, int, int);
  void SetWindowIcon(const std::filesystem::path&);
  Scene* GetActiveScene();
  glm::vec3 GetRandomPosition(float);
  AssetLoader assetLoader;
  SceneManager sceneManager;
protected:
  GLFWwindow* window = nullptr;
  float deltaTime = .0f;
  unsigned int activeSceneID = 0;
  // TODO: create the necessary API functions and make the following private
  EntityManager assetManager;
  InputManager inputManager;
public:
  Application(const std::string&);
  virtual ~Application() = default;
  const std::string& GetName() const;
  /// <summary>
  /// Start the systems
  /// </summary>
  virtual void Start();
  /// <returns>true if the application is running, false otherwise</returns>
  virtual bool Status();
  /// <summary>
  /// Update application state
  /// </summary>
  virtual void Update();
  /// <summary>
  /// Clean up memory and terminate the application
  /// </summary>
  virtual void Shutdown();
  /// <summary>
  /// Start, then update the application indefinitely until status becomes false
  /// </summary>
  void Run();
  template <typename T, typename... Z>
  T* CreateSystem(Z&&... args);
  template <typename T>
  void DeleteSystem();
  template <typename T>
  T* GetSystem();
  unsigned int CreateScene(const std::string&);
  void DeleteScene(unsigned int);
  void SetActiveScene(unsigned int);
  Camera* GetActiveCamera();
  int CreateEntity(std::string&);
  void DeleteEntity(unsigned int);
  void DeleteEntity(const std::string&);
  void DeleteAllEntities();
  void DeleteAllEntities(const std::string&);
  std::string GetEntityName(unsigned int);
  std::string GetAssetName(unsigned int);
  int GetEntityID(const std::string&);
  int GetAssetID(const std::string&);
  void RenameEntity(unsigned int, std::string&);
  bool AddChildEntity(unsigned int, unsigned int);
  bool EntityHasParent(unsigned int);
  bool EntityHasChildren(unsigned int);
  template <typename T>
  T* AddComponent(unsigned int);
  IComponent* AddComponent(unsigned int, const std::string&);
  template <typename T>
  void RemoveComponent(unsigned int);
  void RemoveComponent(unsigned int, const std::string&);
  template <typename T>
  T* GetComponent(unsigned int);
  IComponent* GetComponent(unsigned int, const std::string&);
  std::vector<IComponent*> GetAllComponents(unsigned int);
  std::vector<std::string> GetMissingComponents(unsigned int);
  template <typename F>
  void ForEachChildOfEntity(unsigned int, F);
  template <typename F>
  void ForEachRootEntity(F);
  template <typename F>
  void ForEachRootAsset(F);
  template <typename F>
  void ForAllEntities(F);
  int Spawn(std::string&, int = -1, bool = false, float = 10.0f);
  void SpawnMulti(const std::string&, int, float);
  bool GetKey(int) const;
  bool GetButton(int) const;
  void SetKeyCallback(int, int, std::function<void()>, std::string = "");
  void UnsetKeyCallback(int, int);
  int LoadModel(const std::filesystem::path&);
  int LoadPrimitive(PrimitiveID);
  int LoadCubeMap(std::string&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&);
};
template <typename T, typename... Z>
T* Application::CreateSystem(Z&&... args) {
  static_assert(std::is_base_of_v<System, T>, "T must extend System.");
  auto system = new T(std::forward<Z>(args)...);
  systems.push_back(static_cast<System*>(system));
  return system;
}
template <typename T>
void Application::DeleteSystem() {
  auto it = std::remove_if(systems.begin(), systems.end(), [](System* system) {
    if (auto systemT = static_cast<T*>(system)) {
      delete systemT;
      return true;
    }
    return false;
  });
  systems.erase(it, systems.end());
}
template <typename T>
T* Application::GetSystem() {
  for (auto system : systems)
    if (auto systemT = static_cast<T*>(system))
      return systemT;
  return nullptr;
}
template <typename T>
T* Application::AddComponent(unsigned int id) {
  auto scene = GetActiveScene();
  if (!scene)
    return nullptr;
  return scene->GetEntityManager().AddComponent<T>(id);
}
template <typename T>
void Application::RemoveComponent(unsigned int id) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().RemoveComponent<T>(id);
}
template <typename T>
T* Application::GetComponent(unsigned int id) {
  auto scene = GetActiveScene();
  if (!scene)
    return nullptr;
  return scene->GetEntityManager().GetComponent<T>(id);
}
template <typename F>
void Application::ForEachChildOfEntity(unsigned int parent, F func) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().ForEachChild(parent, func);
}
template <typename F>
void Application::ForEachRootEntity(F func) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().ForEachRoot(func);
}
template <typename F>
void Application::ForEachRootAsset(F func) {
  assetManager.ForEachRoot(func);
}
template <typename F>
void Application::ForAllEntities(F func) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().ForAll(func);
}
