#pragma once
#include <app_config.hpp>
#include <asset_loader.hpp>
#include <command_manager.hpp>
#include <entity_manager.hpp>
#include <id.hpp>
#include <input_manager.hpp>
#include <kuki_engine_export.h>
#include <primitive.hpp>
#include <scene.hpp>
#include <scene_manager.hpp>
#include <system.hpp>
#include <vector>
namespace kuki {
class KUKI_ENGINE_API Application {
private:
  AppConfig config;
  std::vector<System*> systems;
  static void WindowCloseCallback(GLFWwindow*);
  static void FramebufferSizeCallback(GLFWwindow*, int, int);
  static void DebugMessageCallback(unsigned int, unsigned int, unsigned int, unsigned int, int, const char*, const void*);
  static void CursorPosCallback(GLFWwindow*, double, double);
  static void KeyCallback(GLFWwindow*, int, int, int, int);
  static void CharCallback(GLFWwindow*, unsigned int);
  static void MouseButtonCallback(GLFWwindow*, int, int, int);
  void SetWindowIcon(const std::filesystem::path&);
  AssetLoader assetLoader;
  CommandManager commandManager;
  EntityManager assetManager;
  InputManager inputManager;
  SceneManager sceneManager;
protected:
  GLFWwindow* window{};
  float deltaTime{};
  size_t activeSceneId{};
public:
  Application(const AppConfig&);
  virtual ~Application() = default;
  const std::string& GetName() const;
  void Configure(const AppConfig&);
  const AppConfig& GetConfig() const;
  /// @brief Initialize app window, graphics API, and the systems
  virtual void Init();
  /// @brief Start the systems
  virtual void Start();
  /// @return true if the application is running, false otherwise
  virtual bool Status();
  /// @brief Update application state
  virtual void Update();
  virtual void LateUpdate();
  /// @brief Clean up memory and terminate the application
  virtual void Shutdown();
  /// @brief Start, then update the application indefinitely until status becomes false
  void Run();
  void RegisterCommand(ICommand*);
  void UnregisterCommand(const std::string&);
  int DispatchCommand(const std::string&, std::string&);
  template <typename T, typename... Z>
  requires std::derived_from<T, System>
  T* CreateSystem(Z&&...);
  template <typename T>
  void DeleteSystem();
  template <typename T>
  T* GetSystem();
  size_t CreateScene(const std::string&);
  void DeleteScene(size_t);
  void SetActiveScene(size_t);
  Scene* GetActiveScene();
  Camera* GetActiveCamera();
  ID CreateEntity(std::string&);
  ID CreateAsset(std::string&);
  void DeleteEntity(const ID);
  void DeleteAsset(const ID);
  void DeleteEntity(const std::string&);
  void DeleteAllEntities();
  void DeleteAllEntities(const std::string&);
  size_t GetEntityCount();
  std::string GetEntityName(const ID);
  std::string GetAssetName(const ID);
  bool IsEntity(const ID);
  ID GetEntityId(const std::string&);
  ID GetAssetId(const std::string&);
  void RenameEntity(const ID, std::string&);
  bool AddChildEntity(const ID, const ID);
  bool EntityHasParent(const ID);
  bool EntityHasChildren(const ID);
  template <typename T>
  T* AddEntityComponent(const ID);
  template <typename T>
  T* AddAssetComponent(const ID);
  IComponent* AddEntityComponent(const ID, const std::string&);
  template <typename T>
  void RemoveEntityComponent(const ID);
  void RemoveEntityComponent(const ID, ComponentType);
  void RemoveEntityComponent(const ID, const std::string&);
  template <typename T>
  T* GetEntityComponent(const ID);
  template <typename T>
  T* GetAssetComponent(const ID);
  template <typename... T>
  std::tuple<T*...> GetEntityComponents(const ID);
  template <typename... T>
  std::tuple<T*...> GetAssetComponents(const ID);
  IComponent* GetEntityComponent(const ID, ComponentType);
  IComponent* GetEntityComponent(const ID, const std::string&);
  std::vector<IComponent*> GetAllEntityComponents(const ID);
  std::vector<std::string> GetMissingEntityComponents(const ID);
  void SortEntityTransforms();
  void UpdateEntityTransforms();
  template <typename... T, typename F>
  void ForFirstEntity(F&&);
  template <typename... T, typename F>
  void ForEachEntity(F&&);
  template <typename... T, typename F>
  void ForEachAsset(F&&);
  template <typename F>
  void ForEachChildEntity(const ID, F&&);
  template <typename F>
  void ForEachChildAsset(const ID, F&&);
  template <typename F>
  void ForEachRootEntity(F&&);
  template <typename F>
  void ForEachRootAsset(F&&);
  template <typename F>
  void ForAllEntities(F&&);
  template <typename F>
  void ForEachVisibleEntity(const Camera&, F&&);
  template <typename F>
  void ForEachOctreeNode(F&&);
  template <typename F>
  void ForEachOctreeLeafNode(F&&);
  template <typename... T>
  bool EntityHasComponents(const ID);
  template <typename... T>
  bool AssetHasComponents(const ID);
  template <typename T>
  bool AssetHasComponent(const ID);
  size_t GetFPS();
  // NOTE: key and button variants of the following functions are just for convenience; there is no such distinction on InputManager side – they are given non-overlaping IDs
  bool GetKey(int) const;
  bool GetKeyDown(int) const;
  bool GetKeyUp(int) const;
  bool GetButton(int) const;
  bool GetButtonDown(int) const;
  bool GetButtonUp(int) const;
  glm::vec2 GetMousePos() const;
  glm::vec2 GetWASDKeys() const;
  glm::vec2 GetArrowKeys() const;
  void EnableKeys();
  void DisableKeys();
  void EnableButtons();
  void DisableButtons();
  void EnableInputs();
  void DisableInputs();
  void RegisterInputAction(int, InputAction, bool = true);
  void RegisterInputAction(const std::string&, InputAction);
  void UnregisterInputAction(int, bool = true);
  void UnregisterInputAction(const std::string&);
  // TODO: move the following four functions to AssetLoader
  Texture CreateCubeMapFromEquirect(Texture);
  Texture CreateIrradianceMapFromCubeMap(Texture);
  Texture CreatePrefilterMapFromCubeMap(Texture);
  Texture CreateBRDF_LUT();
  void LoadModelAsync(const std::filesystem::path&);
  void LoadTextureAsync(const std::filesystem::path&, TextureType = TextureType::Albedo);
  ID LoadPrimitive(PrimitiveType);
};
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
T* Application::AddEntityComponent(const ID id) {
  auto scene = GetActiveScene();
  if (!scene)
    return nullptr;
  return scene->entityManager.AddComponent<T>(id);
}
template <typename T>
T* Application::AddAssetComponent(const ID id) {
  return assetManager.AddComponent<T>(id);
}
template <typename T>
void Application::RemoveEntityComponent(const ID id) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->entityManager.RemoveComponent<T>(id);
}
template <typename... T>
bool Application::AssetHasComponents(const ID id) {
  return assetManager.HasComponents<T...>(id);
}
template <typename T>
bool Application::AssetHasComponent(const ID id) {
  return assetManager.HasComponent<T>(id);
}
template <typename... T>
bool Application::EntityHasComponents(const ID id) {
  auto scene = GetActiveScene();
  if (!scene)
    return false;
  return scene->entityManager.HasComponents<T...>(id);
}
template <typename T>
T* Application::GetEntityComponent(const ID id) {
  auto scene = GetActiveScene();
  if (!scene)
    return nullptr;
  return scene->entityManager.GetComponent<T>(id);
}
template <typename T>
T* Application::GetAssetComponent(const ID id) {
  return assetManager.GetComponent<T>(id);
}
template <typename... T, typename F>
void Application::ForFirstEntity(F&& func) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->entityManager.ForFirst<T...>(func);
}
template <typename... T, typename F>
void Application::ForEachEntity(F&& func) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->entityManager.ForEach<T...>(func);
}
template <typename... T, typename F>
void Application::ForEachAsset(F&& func) {
  assetManager.ForEach<T...>(func);
}
template <typename F>
void Application::ForEachChildEntity(const ID parent, F&& func) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->entityManager.ForEachChild(parent, func);
}
template <typename F>
void Application::ForEachChildAsset(const ID parent, F&& func) {
  assetManager.ForEachChild(parent, func);
}
template <typename F>
void Application::ForEachRootEntity(F&& func) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->entityManager.ForEachRoot(func);
}
template <typename F>
void Application::ForEachRootAsset(F&& func) {
  assetManager.ForEachRoot(func);
}
template <typename F>
void Application::ForAllEntities(F&& func) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->entityManager.ForAll(func);
}
template <typename F>
void Application::ForEachVisibleEntity(const Camera& camera, F&& func) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->ForEachVisibleEntity(camera, func);
}
template <typename F>
void Application::ForEachOctreeNode(F&& func) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->ForEachOctreeNode(func);
}
template <typename F>
void Application::ForEachOctreeLeafNode(F&& func) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->ForEachOctreeLeafNode(func);
}
template <typename... T>
std::tuple<T*...> Application::GetEntityComponents(const ID id) {
  return std::make_tuple(GetEntityComponent<T>(id)...);
}
template <typename... T>
std::tuple<T*...> Application::GetAssetComponents(const ID id) {
  return std::make_tuple(assetManager.GetComponent<T>(id)...);
}
} // namespace kuki
