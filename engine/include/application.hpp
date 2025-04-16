#pragma once
#include <app_config.hpp>
#include <asset_loader.hpp>
#include <command_manager.hpp>
#include <input_manager.hpp>
#include <kuki_export.h>
#include <scene.hpp>
#include <scene_manager.hpp>
#include <system.hpp>
#include <vector>
namespace kuki {
class KUKI_API Application {
private:
  std::string name;
  AppConfig config;
  std::vector<System*> systems;
  void Init();
  static void WindowCloseCallback(GLFWwindow*);
  static void FramebufferSizeCallback(GLFWwindow*, int, int);
  static void CursorPosCallback(GLFWwindow*, double, double);
  static void KeyCallback(GLFWwindow*, int, int, int, int);
  static void MouseButtonCallback(GLFWwindow*, int, int, int);
  void SetWindowIcon(const std::filesystem::path&);
  glm::vec3 GetRandomPosition(float);
  AssetLoader assetLoader;
  SceneManager sceneManager;
  CommandManager commandManager;
protected:
  GLFWwindow* window{};
  double deltaTime{};
  unsigned int activeSceneId{};
  // TODO: create the necessary API functions and make the following private
  EntityManager assetManager;
  InputManager inputManager;
public:
  Application(const std::string&);
  virtual ~Application() = default;
  const std::string& GetName() const;
  void Configure(const AppConfig&);
  const AppConfig& GetConfig() const;
  /// @brief Start the systems
  virtual void Start();
  /// @return true if the application is running, false otherwise
  virtual bool Status();
  /// @brief Update application state
  virtual void Update();
  /// @brief Clean up memory and terminate the application
  virtual void Shutdown();
  /// @brief Start, then update the application indefinitely until status becomes false
  void Run();
  void RegisterCommand(ICommand*);
  void UnregisterCommand(const std::string&);
  int DispatchCommand(const std::string&, std::string&);
  template <typename T, typename... Z>
  requires std::derived_from<T, System>
  T* CreateSystem(Z&&... args);
  template <typename T>
  void DeleteSystem();
  template <typename T>
  T* GetSystem();
  unsigned int CreateScene(const std::string&);
  void DeleteScene(unsigned int);
  void SetActiveScene(unsigned int);
  Scene* GetActiveScene();
  Camera* GetActiveCamera();
  int CreateEntity(std::string&);
  void DeleteEntity(unsigned int);
  void DeleteEntity(const std::string&);
  void DeleteAllEntities();
  void DeleteAllEntities(const std::string&);
  std::string GetEntityName(unsigned int);
  std::string GetAssetName(unsigned int);
  int GetEntityId(const std::string&);
  int GetAssetId(const std::string&);
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
  template <typename T>
  T* GetAssetComponent(unsigned int);
  template <typename... T>
  std::tuple<T*...> GetComponents(unsigned int);
  template <typename... T>
  std::tuple<T*...> GetAssetComponents(unsigned int);
  IComponent* GetComponent(unsigned int, const std::string&);
  std::vector<IComponent*> GetAllComponents(unsigned int);
  std::vector<std::string> GetMissingComponents(unsigned int);
  template <typename... T, typename F>
  void ForEachEntity(F);
  template <typename F>
  void ForEachChildEntity(unsigned int, F);
  template <typename F>
  void ForEachChildAsset(unsigned int, F);
  template <typename F>
  void ForEachRootEntity(F);
  template <typename F>
  void ForEachRootAsset(F);
  template <typename F>
  void ForAllEntities(F);
  template <typename F>
  void ForEachVisibleEntity(const Camera&, F);
  template <typename F>
  void ForEachOctreeNode(F);
  template <typename F>
  void ForEachOctreeLeafNode(F);
  template <typename... T>
  bool HasComponents(unsigned int);
  template <typename... T>
  bool AssetHasComponents(unsigned int);
  template <typename T>
  bool AssetHasComponent(unsigned int);
  int Spawn(std::string&, int = -1, bool = false, float = 10.0f);
  void SpawnMulti(const std::string&, int, float);
  bool GetKey(int) const;
  bool GetButton(int) const;
  template <typename... T>
  void DisableKeys(T...);
  template <typename... T>
  void EnableKeys(T...);
  void EnableAllKeys();
  void DisableAllKeys();
  void RegisterInputCallback(int, int, std::function<void()>, std::string = "");
  void UnregisterInputCallback(int, int);
  int LoadModel(const std::filesystem::path&);
  int LoadPrimitive(PrimitiveId);
  int LoadCubeMap(std::string&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&);
  template <typename T, typename F>
  int RegisterEventCallback(F&&);
  template <typename T>
  void UnregisterEventCallback(unsigned int);
};
template <typename... T>
void Application::DisableKeys(T... args) {
  inputManager.DisableKeys(args...);
}
template <typename... T>
void Application::EnableKeys(T... args) {
  inputManager.EnableKeys(args...);
}
template <typename T, typename... Z>
requires std::derived_from<T, System>
T* Application::CreateSystem(Z&&... args) {
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
template <typename... T>
bool Application::AssetHasComponents(unsigned int id) {
  return assetManager.HasComponents<T...>(id);
}
template <typename T>
bool Application::AssetHasComponent(unsigned int id) {
  return assetManager.HasComponent<T>(id);
}
template <typename... T>
bool Application::HasComponents(unsigned int id) {
  auto scene = GetActiveScene();
  if (!scene)
    return false;
  return scene->GetEntityManager().HasComponents<T...>(id);
}
template <typename T>
T* Application::GetComponent(unsigned int id) {
  auto scene = GetActiveScene();
  if (!scene)
    return nullptr;
  return scene->GetEntityManager().GetComponent<T>(id);
}
template <typename T>
T* Application::GetAssetComponent(unsigned int id) {
  return assetManager.GetComponent<T>(id);
}
template <typename... T, typename F>
void Application::ForEachEntity(F func) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().ForEach<T...>(func);
}
template <typename F>
void Application::ForEachChildEntity(unsigned int parent, F func) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().ForEachChild(parent, func);
}
template <typename F>
void Application::ForEachChildAsset(unsigned int parent, F func) {
  assetManager.ForEachChild(parent, func);
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
template <typename F>
void Application::ForEachVisibleEntity(const Camera& camera, F func) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().ForEachVisibleEntity(camera, func);
}
template <typename F>
void Application::ForEachOctreeNode(F func) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().ForEachOctreeNode(func);
}
template <typename F>
void Application::ForEachOctreeLeafNode(F func) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().ForEachOctreeLeafNode(func);
}
template <typename... T>
std::tuple<T*...> Application::GetComponents(unsigned int id) {
  return std::make_tuple(GetComponent<T>(id)...);
}
template <typename... T>
std::tuple<T*...> Application::GetAssetComponents(unsigned int id) {
  return std::make_tuple(assetManager.GetComponent<T>(id)...);
}
template <typename T, typename F>
int Application::RegisterEventCallback(F&& callback) {
  auto scene = GetActiveScene();
  if (!scene)
    return -1;
  return scene->GetEntityManager().RegisterCallback<T>(callback);
}
template <typename T>
void Application::UnregisterEventCallback(unsigned int id) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().UnregisterCallback<T>();
}
} // namespace kuki
