#pragma once
#include <application_description.hpp>
#include <application_settings.hpp>
#include <asset_manager.hpp>
#include <component_type.hpp>
#include <concepts.hpp>
#include <entity_manager.hpp>
#include <id.hpp>
#include <input_manager.hpp>
#include <kuki_engine_export.h>
#include <memory>
#include <primitive.hpp>
#include <scene.hpp>
#include <scene_manager.hpp>
#include <system.hpp>
#include <typeindex>
#include <vector>
#include <whereami.h>
namespace kuki {
class KUKI_ENGINE_API Application {
public:
  Application(ApplicationDescription = {});
  virtual ~Application();
  auto Run() -> void;
  virtual auto Init() -> void;
  virtual auto Start() -> void;
  virtual auto Status() -> bool;
  virtual auto Update() -> void;
  virtual auto LateUpdate() -> void;
  virtual auto Shutdown() -> void;
  auto AddChildEntity(const EntityID, const EntityID) -> bool;
  auto AddEntityComponent(const EntityID, const ComponentType) -> void;
  auto CreateEntity(std::string = "") -> EntityID;
  auto CreateScene(std::string = "") -> SceneID;
  auto DeleteEntities() -> void;
  auto DeleteEntity(const EntityID) -> void;
  auto DeleteScene(const SceneID) -> bool;
  auto DisableButtons() -> void;
  auto DisableInputs() -> void;
  auto DisableKeys() -> void;
  auto EnableButtons() -> void;
  auto EnableInputs() -> void;
  auto EnableKeys() -> void;
  auto EntityHasChildren(const EntityID) const -> bool;
  auto EntityHasParent(const EntityID) const -> bool;
  auto GetArrowKeys() const -> glm::vec2;
  auto GetAssetName(const AssetID) const -> std::string;
  auto GetButton(int) const -> bool;
  auto GetButtonDown(int) const -> bool;
  auto GetButtonUp(int) const -> bool;
  auto GetEntityComponent(const EntityID, const ComponentType) -> std::optional<ComponentVariant>;
  auto GetEntityComponentTypes(const EntityID) const -> std::vector<ComponentType>;
  auto GetEntityCount() const -> size_t;
  auto GetEntityName(const EntityID) const -> std::string;
  auto GetFPS() const -> size_t;
  auto GetInfo() const -> const ApplicationDescription &;
  auto GetKey(int) const -> bool;
  auto GetKeyDown(int) const -> bool;
  auto GetKeyUp(int) const -> bool;
  auto GetMissingEntityComponents(const EntityID) const -> std::vector<ComponentType>;
  auto GetMousePos() const -> glm::vec2;
  auto GetName() const -> std::string;
  auto GetSettings() const -> const ApplicationSettings &;
  auto GetWASDKeys() const -> glm::vec2;
  auto InstantiateAsset(const AssetID) -> EntityID;
  auto IsEntity(const EntityID) const -> bool;
  auto LoadAsset(const std::filesystem::path &) -> AssetID;
  auto LoadAssetAsync(const std::filesystem::path &) -> AssetID;
  auto RegisterInputAction(std::string, InputAction) -> void;
  auto RegisterInputAction(int, InputAction, bool = true) -> void;
  auto RemoveEntityComponent(const EntityID, const ComponentType) -> bool;
  auto RenameEntity(const EntityID, std::string) -> bool;
  auto UnloadAsset(const AssetID) -> bool;
  auto UnregisterInputAction(const std::string &) -> void;
  auto UnregisterInputAction(int, bool = true) -> void;
  // templates
  auto ForEachAsset(this auto &, const AssetType, auto &&) -> void;
  auto ForEachAssetPerType(this auto &, auto &&) -> void;
  auto ForEachAssetType(this auto &, auto &&) -> void;
  auto ForEachChildEntity(this auto &, const EntityID, auto &&) -> void;
  auto ForEachPrefab(this auto &, auto &&) -> void;
  auto ForEachRootEntity(this auto &, auto &&) -> void;
  auto GetActiveCamera(this auto &self) -> ConstCorrectPointer<decltype(self), Camera>;
  auto GetActiveScene(this auto &self) -> ConstCorrectPointer<decltype(self), Scene>;
  auto GetAsset(this auto &self, const AssetID) -> ConstCorrectPointer<decltype(self), Asset>;
  template <typename... T>
  auto AddEntityComponent(const EntityID) -> decltype(auto);
  template <IsSystem T, typename... Args>
  auto CreateSystem(Args &&...) -> T *;
  template <IsSystem T>
  auto DeleteSystem() -> bool;
  template <typename... T>
  auto EntityHasComponent(const EntityID) const -> bool;
  template <IsAsset... T>
  auto ForEachAsset(this auto &, auto &&) -> void;
  template <typename... T>
  auto ForEachEntity(this auto &, auto &&) -> void;
  template <typename... T>
  auto ForFirstEntity(this auto &, auto &&) -> void;
  template <IsAsset T>
  auto GetAsset(const AssetID) -> decltype(auto);
  template <typename... T>
  auto GetEntityComponent(this auto &, const EntityID) -> decltype(auto);
  template <IsSystem T>
  auto GetSystem(this auto &) -> decltype(auto);
  template <typename... T>
  auto RemoveEntityComponent(const EntityID) -> bool;
protected:
  GLFWwindow *window{};
  float deltaTime{};
  ApplicationDescription desc;
  ApplicationSettings settings;
private:
  AssetManager assetManager;
  InputManager inputManager;
  SceneManager sceneManager;
  auto InitGL() -> void;
  auto GetExePath() -> std::filesystem::path;
  auto SetWindowIcon() -> void;
  static void CharCallback(GLFWwindow *, unsigned int);
  static void CursorPosCallback(GLFWwindow *, double, double);
  static void DebugMessageCallback(unsigned int, unsigned int, unsigned int, unsigned int, int, const char *, const void *);
  static void FramebufferSizeCallback(GLFWwindow *, int, int);
  static void KeyCallback(GLFWwindow *, int, int, int, int);
  static void MouseButtonCallback(GLFWwindow *, int, int, int);
  static void WindowCloseCallback(GLFWwindow *);
  // TODO: create a system manager
  std::unordered_map<std::type_index, std::unique_ptr<System>> typeIndexToSystem;
};
auto Application::ForEachAsset(this auto &self, const AssetType type, auto &&func) -> void {
  self.assetManager.ForEach(type, std::forward<decltype(func)>(func));
}
auto Application::ForEachAssetPerType(this auto &self, auto &&func) -> void {
  self.assetManager.ForEachPerType(std::forward<decltype(func)>(func));
}
auto Application::ForEachAssetType(this auto &self, auto &&func) -> void {
  self.assetManager.ForEachType(std::forward<decltype(func)>(func));
}
auto Application::ForEachChildEntity(this auto &self, const EntityID parent, auto &&func) -> void {
  if (auto scene = self.GetActiveScene(); scene)
    scene->ForEachChildEntity(parent, std::forward<decltype(func)>(func));
}
auto Application::ForEachPrefab(this auto &self, auto &&func) -> void {
  self.assetManager.ForEachPrefab(std::forward<decltype(func)>(func));
}
auto Application::ForEachRootEntity(this auto &self, auto &&func) -> void {
  if (auto scene = self.GetActiveScene(); scene)
    scene->ForEachRootEntity(std::forward<decltype(func)>(func));
}
auto Application::GetActiveCamera(this auto &self) -> ConstCorrectPointer<decltype(self), Camera> {
  if (auto scene = self.GetActiveScene(); scene)
    return scene->GetActiveCamera();
  return nullptr;
}
auto Application::GetActiveScene(this auto &self) -> ConstCorrectPointer<decltype(self), Scene> {
  return self.sceneManager.GetActive();
}
auto Application::GetAsset(this auto &self, const AssetID id) -> ConstCorrectPointer<decltype(self), Asset> {
  return self.assetManager.Get(id);
}
template <typename... T>
auto Application::AddEntityComponent(const EntityID id) -> decltype(auto) {
  static_assert(sizeof...(T) > 0, "`AddEntityComponent` requires at least one type parameter.");
  if (auto scene = GetActiveScene(); scene)
    return scene->template AddEntityComponent<T...>(id);
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    return static_cast<C *>(nullptr);
  } else
    return std::tuple(static_cast<T *>(nullptr)...);
}
template <IsSystem T, typename... Args>
auto Application::CreateSystem(Args &&...args) -> T * {
  const auto typeIndex = std::type_index(typeid(T));
  if (auto it = typeIndexToSystem.find(typeIndex); it != typeIndexToSystem.end())
    return static_cast<T *>(it->second.get());
  auto [it, _] = typeIndexToSystem.emplace(typeIndex, std::make_unique<T>(std::forward<Args>(args)...));
  return static_cast<T *>(it->second.get());
}
template <IsSystem T>
auto Application::DeleteSystem() -> bool {
  const auto typeIndex = std::type_index(typeid(T));
  return typeIndexToSystem.erase(typeIndex);
}
template <typename... T>
auto Application::EntityHasComponent(const EntityID id) const -> bool {
  if (auto scene = GetActiveScene(); scene)
    return scene->template EntityHasComponent<T...>(id);
  return false;
}
template <IsAsset... T>
auto Application::ForEachAsset(this auto &self, auto &&func) -> void {
  self.assetManager.template ForEach<T...>(std::forward<decltype(func)>(func));
}
template <typename... T>
auto Application::ForEachEntity(this auto &self, auto &&func) -> void {
  if (auto scene = self.GetActiveScene(); scene)
    scene->template ForEachEntity<T...>(std::forward<decltype(func)>(func));
}
template <typename... T>
auto Application::ForFirstEntity(this auto &self, auto &&func) -> void {
  if (auto scene = self.GetActiveScene(); scene)
    scene->template ForFirstEntity<T...>(std::forward<decltype(func)>(func));
}
template <IsAsset T>
auto Application::GetAsset(const AssetID id) -> decltype(auto) {
  return assetManager.Get<T>(id);
}
template <typename... T>
auto Application::GetEntityComponent(this auto &self, const EntityID id) -> decltype(auto) {
  if (auto scene = self.GetActiveScene(); scene)
    return scene->template GetEntityComponent<T...>(id);
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    return static_cast<ConstCorrectPointer<decltype(self), C>>(nullptr);
  } else
    return std::tuple(static_cast<ConstCorrectPointer<decltype(self), T>>(nullptr)...);
}
template <IsSystem T>
auto Application::GetSystem(this auto &self) -> decltype(auto) {
  const auto typeIndex = std::type_index(typeid(T));
  if (auto it = self.typeIndexToSystem.find(typeIndex); it != self.typeIndexToSystem.end())
    return static_cast<ConstCorrectPointer<decltype(self), T>>(it->second.get());
  return static_cast<ConstCorrectPointer<decltype(self), T>>(nullptr);
}
template <typename... T>
auto Application::RemoveEntityComponent(const EntityID id) -> bool {
  if (auto scene = GetActiveScene(); scene)
    return scene->template RemoveEntityComponent<T...>(id);
  return false;
}
} // namespace kuki
