#pragma once
#include <application_description.hpp>
#include <asset_manager.hpp>
#include <component_type.hpp>
#include <concepts.hpp>
#include <entity_manager.hpp>
#include <id.hpp>
#include <input_manager.hpp>
#include <kuki_engine_export.h>
#include <primitive.hpp>
#include <render_target.hpp>
#include <scene.hpp>
#include <scene_manager.hpp>
#include <shader_asset.hpp>
#include <string_view>
#include <system.hpp>
#include <typeindex>
#include <vector>
#include <whereami.h>
namespace kuki {
class RenderingSystem;
class KUKI_ENGINE_API Application {
public:
  Application(ApplicationDescription = {});
  virtual ~Application();
  auto Run() -> void;
  virtual auto Start() -> void;
  virtual auto Update(const float) -> void;
  virtual auto Shutdown() -> void;
  virtual auto Status() -> bool;
  Event<Scene &> OnSceneLoaded;
  auto AddChildEntity(const EntityID, const EntityID) -> bool;
  auto AlignView(const EntityID) -> void;
  auto CreateEntity(std::string = "") -> EntityID;
  auto CreateScene(std::string = "") -> SceneID;
  auto DeleteEntities() -> void;
  auto DeleteEntity(const EntityID) -> void;
  auto DeltaTime() const -> float;
  auto DisableButtons() -> void;
  auto DisableInputs() -> void;
  auto DisableKeys() -> void;
  auto EnableButtons() -> void;
  auto EnableInputs() -> void;
  auto EnableKeys() -> void;
  auto EntityHasChildren(const EntityID) const -> bool;
  auto EntityHasParent(const EntityID) const -> bool;
  auto ForEachAsset(this auto &, const AssetType, auto &&) -> void;
  auto ForEachAssetType(this auto &, auto &&) -> void;
  auto ForEachChildEntity(this auto &, const EntityID, auto &&) -> void;
  auto ForEachRootEntity(this auto &, auto &&) -> void;
  auto GetArrowKeys() const -> glm::ivec2;
  auto GetAsset(this auto &self, const AssetID) -> ConstCorrectPointer<decltype(self), Asset>;
  auto GetAsset(this auto &self, const std::string &) -> ConstCorrectPointer<decltype(self), Asset>;
  auto GetAssetName(const AssetID) const -> std::string;
  auto GetAssetPath(const AssetID) const -> std::filesystem::path;
  auto GetAssetType(const AssetID) const -> AssetType;
  auto GetButton(int) const -> bool;
  auto GetButtonDown(int) const -> bool;
  auto GetButtonUp(int) const -> bool;
  auto GetCamera(this auto &self, const std::string & = "") -> ConstCorrectPointer<decltype(self), Camera>;
  auto GetDescription() const -> const ApplicationDescription &;
  auto GetEntityComponent(const EntityID, const ComponentType) -> std::optional<ComponentVariant>;
  auto GetEntityComponentTypes(const EntityID) const -> std::vector<ComponentType>;
  auto GetEntityCount() const -> size_t;
  auto GetEntityName(const EntityID) const -> std::string;
  auto GetFPS() -> size_t;
  auto GetKey(int) const -> bool;
  auto GetKeyDown(int) const -> bool;
  auto GetKeyUp(int) const -> bool;
  auto GetEntityParent(const EntityID) const -> EntityID;
  auto GetMissingEntityComponents(const EntityID) const -> std::vector<ComponentType>;
  auto GetMousePosition() const -> glm::vec2;
  auto GetName() const -> std::string;
  auto GetScene(this auto &, const std::string & = "") -> decltype(auto);
  auto GetScrollOffset() const -> glm::vec2;
  auto GetWASDKeys() const -> glm::ivec2;
  auto InstantiateAsset(const AssetID) -> EntityID;
  auto InstantiateAsset(const std::string &) -> EntityID;
  auto IsAssetLoaded(const AssetID) const -> bool;
  auto IsEntity(const EntityID) const -> bool;
  auto IsViewportHovered() const -> bool;
  auto ResolveModelInstance(const EntityID, const AssetID) -> void;
  auto LoadComputeFromSource(const std::string_view, std::string = "") -> AssetID;
  auto LoadPrimitive(const std::string &) -> void;
  auto LoadShader(const std::filesystem::path &, const std::filesystem::path &, std::string name = "", const MaterialType = MaterialType::Unlit) -> AssetID;
  auto LoadShaderFromSource(const std::string_view, const std::string_view, std::string name = "", const MaterialType = MaterialType::Unlit) -> AssetID;
  auto GetPreviewSize() -> int;
  auto PreviewAsset(const AssetID) -> RenderTarget *;
  auto SetPreviewSize(const int) -> void;
  auto BeginKeyCapture() -> void;
  auto CancelKeyCapture() -> void;
  auto GetBinding(const std::string &) const -> InputManager::Trigger;
  auto GetBindingDescription(const std::string &) const -> std::string;
  auto GetBindingNames() const -> const std::vector<std::string> &;
  auto GetSequenceDescription(const std::string &) const -> std::string;
  auto GetSequenceNames() const -> const std::vector<std::string> &;
  auto GetTriggerName(const InputManager::Trigger &) const -> std::string;
  virtual auto GetSelectedEntity() const -> EntityID {
    return {};
  }
  virtual auto GetSelectedEntities() const -> std::vector<EntityID> {
    const auto id = GetSelectedEntity();
    if (!id)
      return {};
    return {id};
  }
  auto IsBindingHeld(const std::string &) const -> bool;
  auto IsBindingPressed(const std::string &) const -> bool;
  auto IsSequenceInProgress() const -> bool;
  auto PollKeyCapture() -> InputManager::CaptureOutcome;
  auto RegisterBinding(const std::string &, const InputManager::Trigger &, std::string = "") -> void;
  auto RegisterInputAction(const std::string &, InputAction, std::string = "") -> InputManager::ActionID;
  auto RegisterInputAction(int, InputAction, bool = true) -> InputManager::ActionID;
  auto RemoveEntityScript(const EntityID, const std::type_index) -> bool;
  auto RenameAsset(const AssetID, std::string = "") -> bool;
  auto RenameEntity(const EntityID, std::string) -> bool;
  auto SetActiveCamera(const EntityID, const std::string & = "") -> bool;
  auto SetBinding(const std::string &, const InputManager::Trigger &) -> void;
  auto UnregisterInputAction(InputManager::ActionID) -> bool;
  auto SetCursorLocked(bool) -> void;
  virtual auto SetInputCaptureActive(bool) -> void {}
  auto SetResolution(const int = 1920, const int = 1080) -> void;
  auto SetViewportHovered(bool) -> void;
  template <typename... T>
  auto AddEntityComponent(const EntityID) -> decltype(auto);
  template <IsAsset... T>
  auto ForEachAsset(this auto &, auto &&) -> void;
  template <typename... T>
  auto ForEachEntity(this auto &, auto &&) -> void;
  template <IsAsset T>
  auto GetAsset(const AssetID) -> decltype(auto);
  template <typename... T>
  auto GetEntityComponent(this auto &, const EntityID) -> decltype(auto);
  template <IsAsset T>
  auto LoadAsset(const std::filesystem::path &, std::string = "", const AssetID = {}) -> AssetID;
  template <IsAsset T>
  auto LoadAssetAsync(const std::filesystem::path &, std::string = "", const AssetID = {}) -> AssetID;
  template <typename... T>
  auto RemoveEntityComponent(const EntityID) -> bool;
protected:
  GLFWwindow *window{};
  virtual auto GetRenderingSystem() -> RenderingSystem * {
    return nullptr;
  }
  virtual auto StartSystems() -> void {}
  virtual auto PostStartSystems() -> void {}
  virtual auto UpdateSystems(const float) -> void {}
  virtual auto ShutdownSystems() -> void {}
private:
  ApplicationDescription desc;
  AssetManager assetManager;
  InputManager inputManager;
  SceneManager sceneManager;
  float deltaTime{};
  bool viewportHovered{};
  auto CreateWindow() -> bool;
  auto GetExePath() -> std::filesystem::path;
  auto LoadPrimitiveAssets() -> void;
  auto PreStart() -> void;
  auto PostStart() -> void;
  auto PreUpdate() -> void;
  auto PostUpdate() -> void;
  auto PreShutdown() -> void;
  auto SetWindowIcon() -> void;
  static auto CharCallback(GLFWwindow *, unsigned int) -> void;
  static auto CursorPosCallback(GLFWwindow *, double, double) -> void;
  static auto DebugMessageCallback(unsigned int, unsigned int, unsigned int, unsigned int, int, const char *, const void *) -> void;
  static auto FramebufferSizeCallback(GLFWwindow *, int, int) -> void;
  static auto KeyCallback(GLFWwindow *, int, int, int, int) -> void;
  static auto MouseButtonCallback(GLFWwindow *, int, int, int) -> void;
  static auto ScrollCallback(GLFWwindow *, double, double) -> void;
  static auto WindowCloseCallback(GLFWwindow *) -> void;
};
auto Application::ForEachAsset(this auto &self, const AssetType type, auto &&func) -> void {
  self.assetManager.ForEach(type, std::forward<decltype(func)>(func));
}
auto Application::ForEachAssetType(this auto &self, auto &&func) -> void {
  self.assetManager.ForEachType(std::forward<decltype(func)>(func));
}
auto Application::ForEachChildEntity(this auto &self, const EntityID parent, auto &&func) -> void {
  if (auto scene = self.GetScene(); scene)
    scene->ForEachChildEntity(parent, std::forward<decltype(func)>(func));
}
auto Application::ForEachRootEntity(this auto &self, auto &&func) -> void {
  if (auto scene = self.GetScene(); scene)
    scene->ForEachRootEntity(std::forward<decltype(func)>(func));
}
auto Application::GetAsset(this auto &self, const AssetID id) -> ConstCorrectPointer<decltype(self), Asset> {
  return self.assetManager.Get(id);
}
auto Application::GetAsset(this auto &self, const std::string &name) -> ConstCorrectPointer<decltype(self), Asset> {
  return self.assetManager.Get(name);
}
auto Application::GetCamera(this auto &self, const std::string &name) -> ConstCorrectPointer<decltype(self), Camera> {
  if (auto scene = self.GetScene(name); scene)
    return scene->GetActiveCamera();
  return nullptr;
}
auto Application::GetScene(this auto &self, const std::string &name) -> decltype(auto) {
  return self.sceneManager.Get(name);
}
template <typename... T>
auto Application::AddEntityComponent(const EntityID id) -> decltype(auto) {
  static_assert(sizeof...(T) > 0, "`AddEntityComponent` requires at least one type parameter.");
  if (auto scene = GetScene(); scene)
    return scene->template AddEntityComponent<T...>(id);
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    return static_cast<C *>(nullptr);
  } else
    return std::tuple(AddEntityComponent<T>(id)...);
}
template <IsAsset... T>
auto Application::ForEachAsset(this auto &self, auto &&func) -> void {
  self.assetManager.template ForEach<T...>(std::forward<decltype(func)>(func));
}
template <typename... T>
auto Application::ForEachEntity(this auto &self, auto &&func) -> void {
  if (auto scene = self.GetScene(); scene)
    scene->template ForEachEntity<T...>(std::forward<decltype(func)>(func));
}
template <IsAsset T>
auto Application::GetAsset(const AssetID id) -> decltype(auto) {
  return assetManager.Get<T>(id);
}
template <typename... T>
auto Application::GetEntityComponent(this auto &self, const EntityID id) -> decltype(auto) {
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    if constexpr (std::is_same_v<Script, C>) {
      if (auto scene = self.GetScene(); scene)
        return scene->template GetEntityComponent<T...>(id);
      else
        return std::vector<ConstCorrectPointer<decltype(self), C>>();
    } else {
      if (auto scene = self.GetScene(); scene)
        return scene->template GetEntityComponent<T...>(id);
      else
        return static_cast<ConstCorrectPointer<decltype(self), C>>(nullptr);
    }
  } else {
    if (auto scene = self.GetScene(); scene)
      return scene->template GetEntityComponent<T...>(id);
    else
      return std::tuple(self.template GetEntityComponent<T>(id)...);
  }
}
template <IsAsset T>
auto Application::LoadAsset(const std::filesystem::path &path, std::string name, const AssetID forcedId) -> AssetID {
  return assetManager.Load<T>(path, std::move(name), forcedId);
}
template <IsAsset T>
auto Application::LoadAssetAsync(const std::filesystem::path &path, std::string name, const AssetID forcedId) -> AssetID {
  return assetManager.LoadAsync<T>(path, std::move(name), forcedId);
}
template <typename... T>
auto Application::RemoveEntityComponent(const EntityID id) -> bool {
  if (auto scene = GetScene(); scene)
    return scene->template RemoveEntityComponent<T...>(id);
  return false;
}
} // namespace kuki
