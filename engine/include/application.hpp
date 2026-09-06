#pragma once
#include <application_description.hpp>
#include <asset_manager.hpp>
#include <component_type.hpp>
#include <concepts.hpp>
#include <entity_manager.hpp>
#include <graphics_context.hpp>
#include <id.hpp>
#include <input_manager.hpp>
#include <kuki_engine_export.h>
#include <memory>
#include <overlay.hpp>
#include <preferences.hpp>
#include <primitive.hpp>
#include <scene.hpp>
#include <scene_manager.hpp>
#include <shader_asset.hpp>
#include <string_view>
#include <system.hpp>
#include <vector>
namespace kuki {
class GameBuilder;
class RenderingSystem;
class KUKI_ENGINE_API Application {
public:
  Application(ApplicationDescription = {});
  virtual ~Application();
  /// @brief Brings the application up, runs frames until `Status` says stop, and takes it down.
  ///
  /// The one lifecycle entry point that is public, because `main` has to call something. The four
  /// hooks it drives are protected -- see the note on `Start`.
  auto Run() -> void;
  Event<Scene &> OnSceneLoaded;
  auto AddChildEntity(const EntityID, const EntityID) -> bool;
  auto AlignView(const EntityID) -> void;
  auto CreateEntity(std::string = "") -> EntityID;
  auto CreateScene(std::string = "") -> SceneID;
  /// @brief Makes an existing scene the active one. False when nothing goes by that name.
  auto SwitchScene(const std::string &) -> bool;
  /// @brief Starts a fluent description of a game: scenes, entities, components and scripts.
  ///
  /// The ergonomic path, in place of `CreateScene` / `CreateEntity` / `AddEntityComponent` written
  /// out by hand. One expression describes a whole scene, and the builders track their own nesting
  /// so no call has to be paired with a closing one:
  ///
  ///     Game("Chess").Scene("Main").Entity("Manager").Script<GameManager>().Entity("Pawn").Mesh("Cube");
  ///
  /// `Entity` there asks for scene scope, so the entity frame left open by `Script` is discarded
  /// on the way through. See `BuilderStack` for how that resolution works and for what it cannot
  /// do -- deep hierarchies still want `Child`/`Parent`, and anything genuinely complicated is
  /// clearer built from the calls this wraps.
  ///
  /// The name is the window's title, which is the only effect it can honestly have.
  ///
  /// Declared here but defined in `game_builder.hpp`, which every caller has to include: the
  /// builders need the whole facade, so having the facade know more than their names would be
  /// circular.
  auto Game(const std::string & = "") -> GameBuilder;
  /// @brief Renames the window.
  ///
  /// Worth having for its own sake, and worth more than usual here: with no text rendering in the
  /// engine, the title bar is the only place a game can put a line of status.
  auto SetWindowTitle(const std::string &) -> void;
  /// @brief Hands the asset manager something built rather than loaded, under a name.
  ///
  /// The route in for an asset that has no file to be loaded from: a mesh laid out in code, or a
  /// texture generated at run time. `LoadAsset` cannot serve those -- it is keyed by path, and
  /// there is no path -- which previously left a game with no way to make one at all.
  ///
  /// Give the asset an id from `MakeBuiltInAssetID` rather than a generated one where a scene may
  /// come to reference it, for the reason that function sets out: a generated id differs every run,
  /// and a saved scene naming one resolves to nothing.
  ///
  /// @return Whether it was taken. An asset already known by that id is left alone and refused.
  auto AddAsset(std::unique_ptr<Asset>, std::string = "") -> bool;
  auto DeleteEntities() -> void;
  auto DeleteEntity(const EntityID) -> void;
  auto DeltaTime() const -> float;
  auto EntityHasChildren(const EntityID) const -> bool;
  auto ForEachAsset(this auto &, const AssetType, auto &&) -> void;
  auto ForEachAssetType(this auto &, auto &&) -> void;
  auto ForEachChildEntity(this auto &, const EntityID, auto &&) -> void;
  auto ForEachRootEntity(this auto &, auto &&) -> void;
  /// @brief The direction a key quad is currently pointing, as -1, 0 or 1 per axis.
  auto GetKeyAxis(const InputManager::KeyAxis) const -> glm::ivec2;
  auto GetAsset(this auto &self, const AssetID) -> ConstCorrectPointer<decltype(self), Asset>;
  auto GetAsset(this auto &self, const std::string &) -> ConstCorrectPointer<decltype(self), Asset>;
  auto GetAssetName(const AssetID) const -> std::string;
  auto GetAssetPath(const AssetID) const -> std::filesystem::path;
  auto GetAssetType(const AssetID) const -> AssetType;
  auto GetCamera(this auto &self, const std::string & = "") -> ConstCorrectPointer<decltype(self), Camera>;
  auto GetDescription() const -> const ApplicationDescription &;
  /// @brief The live presentation surface, or null before startup has created it.
  ///
  /// Backends downcast this to reach their own device -- `DXRenderer::GetContext` and the guard at
  /// the top of `GLRenderer::RenderScene` are the two -- and nothing else should.
  ///
  /// That is a convention and not a rule, which is worth saying plainly rather than leaving the
  /// sentence above to imply otherwise. It is public because those two renderers hold an
  /// `Application &` rather than deriving from one, so protected would not reach them and the
  /// alternative is a friend declaration per backend, one of them behind `KUKI_HAS_DIRECTX`.
  /// Public it therefore is, which means every script sees it too, and a script that downcasts to
  /// `DXContext` and drives the command list itself will compile and appear to work.
  ///
  /// If that ever stops being hypothetical, the fix is not to hide this but to give the renderers
  /// their context at construction, which is where they should have had it from.
  auto GetGraphicsContext() const -> GraphicsContext *;
  auto GetEntityComponent(const EntityID, const ComponentType) -> std::optional<ComponentVariant>;
  auto GetEntityComponentTypes(const EntityID) const -> std::vector<ComponentType>;
  auto GetEntityCount() const -> size_t;
  auto GetEntityName(const EntityID) const -> std::string;
  auto GetEntityParent(const EntityID) const -> EntityID;
  auto GetMousePosition() const -> glm::vec2;
  /// @brief Named values this application keeps between runs. See `Preferences`.
  ///
  /// Loaded before `Start` and written back after the systems have shut down, so a script may read
  /// them as it starts and set them as it stops without arranging either itself.
  auto GetPreferences(this auto &self) -> decltype(auto) {
    return (self.preferences);
  }
  /// @brief The text queued to be drawn over this frame's picture. See `Overlay`.
  auto GetOverlay(this auto &self) -> decltype(auto) {
    return (self.overlay);
  }
  /// @brief Bakes a font for the overlay and registers its atlas as a texture asset.
  ///
  /// Both halves belong together, which is why this is here rather than on `Overlay`: the bake is
  /// the overlay's business and the asset is the manager's, and a caller doing the second by hand
  /// would have to know the name the backends look the atlas up by.
  ///
  /// Nothing is drawn until this succeeds -- `Overlay::DrawText` ignores a request made with no
  /// font, rather than queueing text that could never be set.
  auto SetOverlayFont(const std::filesystem::path &, const int = Font::DefaultPixelHeight) -> bool;
  auto GetScene(this auto &, const std::string & = "") -> decltype(auto);
  auto GetScrollOffset() const -> glm::vec2;
  auto InstantiateAsset(const AssetID) -> EntityID;
  auto InstantiateAsset(const std::string &) -> EntityID;
  auto IsAssetLoaded(const AssetID) const -> bool;
  auto IsEntity(const EntityID) const -> bool;
  auto IsViewportHovered() const -> bool;
  auto ResolveModelInstance(const EntityID, const AssetID) -> void;
  auto LoadComputeFromSource(const std::string_view, std::string = "") -> AssetID;
  auto LoadShader(const std::filesystem::path &, const std::filesystem::path &, std::string name = "", const MaterialType = MaterialType::Unlit) -> AssetID;
  auto LoadShaderFromSource(const std::string_view, const std::string_view, std::string name = "", const MaterialType = MaterialType::Unlit) -> AssetID;
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
  /// @brief Whether a key or mouse button is down now, this frame, or came up this frame.
  ///
  /// One trio rather than two. There used to be a `GetKey`/`GetKeyDown`/`GetKeyUp` and a
  /// `GetButton`/`GetButtonDown`/`GetButtonUp`, and the six were three pairs of identical bodies:
  /// keys and buttons share one `std::bitset<256>` per phase inside `InputManager`, indexed by
  /// `GLFWInputToIndex`, so `GetKey(GLFW_MOUSE_BUTTON_LEFT)` did exactly what `GetButton` did. The
  /// split promised a distinction the implementation never made, and the GLFW constant at the call
  /// site already says which half of the space is meant.
  ///
  /// Named to match the `IsBinding...` pair above, which asks the same question of a bound name.
  auto IsInputHeld(const int) const -> bool;
  auto IsInputPressed(const int) const -> bool;
  auto IsInputReleased(const int) const -> bool;
  auto IsSequenceInProgress() const -> bool;
  /// @brief Flags an entity's transform, and everything below it, for recomputation.
  ///
  /// Call after writing to a transform's position, rotation or scale; the component carries no
  /// dirty bit of its own, so an unannounced change goes unapplied.
  auto MarkTransformDirty(const EntityID) -> void;
  /// @brief The entity under a point given in window coordinates, or an invalid id for empty space.
  ///
  /// Reads the picking buffer the scene pass fills, which is why it is exact rather than a ray cast
  /// against bounding volumes: what it returns is the thing whose pixel is actually there.
  ///
  /// Takes window coordinates -- the space `GetMousePosition` reports in -- and converts to the
  /// render target's texels here, because the two are only the same size by coincidence. The editor
  /// does its own conversion instead, from a panel that is a third space again, and calls the
  /// rendering system directly.
  auto PickEntity(const glm::vec2 &) -> EntityID;
  /// @brief The id of the overlay text under a point in window coordinates, or `Overlay::NoHit`.
  ///
  /// The overlay's counterpart of `PickEntity`, and here for the same reason that one is: the
  /// overlay is laid out in the render target's pixels and the mouse is reported in the window's,
  /// and the two are only the same size by coincidence.
  auto PickOverlay(const glm::vec2 &) -> int;
  /// @brief Asks the loop to stop after the current frame.
  ///
  /// The polite half of what the window's close button does, and the only way a game can end
  /// itself: `Status` is a question rather than an instruction, and closing the window was
  /// previously the sole route to a false answer. Sets the same flag the close callback sets, so
  /// shutdown runs exactly as it does when someone closes the window.
  auto Quit() -> void;
  auto RegisterBinding(const std::string &, const InputManager::Trigger &, std::string = "") -> void;
  auto RegisterInputAction(const std::string &, InputAction, std::string = "") -> InputManager::ActionID;
  auto RegisterInputAction(int, InputAction, bool = true) -> InputManager::ActionID;
  auto RenameAsset(const AssetID, std::string = "") -> bool;
  auto RenameEntity(const EntityID, std::string) -> bool;
  auto SetActiveCamera(const EntityID, const std::string & = "") -> bool;
  auto UnregisterInputAction(InputManager::ActionID) -> bool;
  auto SetCursorLocked(bool) -> void;
  /// @brief Turns keys, mouse buttons or both on or off. See `InputManager::SetEnabled`.
  auto SetInputEnabled(const InputManager::InputKind, const bool) -> void;
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
  std::unique_ptr<GraphicsContext> graphicsContext;
  virtual auto GetRenderingSystem() -> RenderingSystem * {
    return nullptr;
  }
  /// @brief The input manager, for a subclass that has to reach past what the facade forwards.
  ///
  /// Here for the editor, which owns the rebinding panel: reading out every binding's name and
  /// description, capturing the next keystroke, and writing the result back is a settings screen's
  /// job, not something an engine facade should carry. Those nine forwards used to be public, which
  /// put a key-capture state machine in front of every script for the sake of one panel.
  ///
  /// Protected rather than public, and a manager rather than nine methods, because a subclass
  /// reaching its own manager is a narrower hole than nine forwards that everything can see. It is
  /// the same trade `GetRenderingSystem` above already makes.
  auto GetInputManager() -> InputManager & {
    return inputManager;
  }
  /// @brief The hooks `Run` drives the application through, for a subclass to fill in.
  ///
  /// Protected rather than public, because nothing holding an `Application` has any business
  /// calling them -- and everything holding one is a script. `Script::Update` is handed the whole
  /// facade by design, so while these were public a script could call `Shutdown` mid-frame or
  /// `Update` a second time within the frame it was called from. Neither is a thing anyone did;
  /// both were a thing the type permitted, and `Run` is the only caller either ever had.
  ///
  /// The access level does not affect overriding, and `Editor` keeps its own overrides private.
  ///
  /// `Run` is still public and still reentrant, since `main` has to reach it and C++ has no way to
  /// say "public to `main` alone". A guard on it would close that, and is deliberately not here:
  /// there is one call site in the whole repository, and a flag defending against a caller who does
  /// not exist reads as though one did.
  virtual auto Start() -> void;
  virtual auto Update(const float) -> void;
  virtual auto Shutdown() -> void;
  /// @brief Whether the loop should run another frame.
  virtual auto Status() -> bool;
  virtual auto StartSystems() -> void {}
  virtual auto PostStartSystems() -> void {}
  virtual auto UpdateSystems(const float) -> void {}
  virtual auto ShutdownSystems() -> void {}
private:
  ApplicationDescription desc;
  AssetManager assetManager;
  Overlay overlay;
  Preferences preferences;
  InputManager inputManager;
  SceneManager sceneManager;
  float deltaTime{};
  bool viewportHovered{};
  auto CreateWindow() -> bool;
  auto GetExePath() -> std::filesystem::path;
  /// @brief Builds one built-in mesh asset by name, if it is not already loaded.
  ///
  /// Private because the only caller is `LoadPrimitiveAssets` below, and by the time anything
  /// outside could ask, every name it accepts is already an asset: `GetAsset("Cube")` is the route
  /// from out there, and it does not need the six-way string dispatch this carries.
  auto LoadPrimitive(const std::string &) -> void;
  auto LoadPrimitiveAssets() -> void;
  auto PreStart() -> void;
  auto PostStart() -> void;
  auto PreUpdate() -> void;
  auto PostUpdate() -> void;
  auto PreShutdown() -> void;
  auto SetWindowIcon() -> void;
  static auto CharCallback(GLFWwindow *, unsigned int) -> void;
  static auto CursorPosCallback(GLFWwindow *, double, double) -> void;
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
/// The `Script` helpers that act on the script's own entity. Declared on `Script`, defined here
/// because they call into `Application` and `script.hpp` only forward declares it -- the call is
/// through a plain `Application *`, which is not a dependent type, so the lookup happens where the
/// body is written rather than where it is instantiated.
template <typename... T>
auto Script::AddComponent() -> decltype(auto) {
  static_assert(sizeof...(T) > 0, "`AddComponent` requires at least one type parameter.");
  if (app && entityId)
    return app->AddEntityComponent<T...>(entityId);
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    return static_cast<C *>(nullptr);
  } else
    return std::tuple(AddComponent<T>()...);
}
template <typename... T>
auto Script::GetComponent() -> decltype(auto) {
  static_assert(sizeof...(T) > 0, "`GetComponent` requires at least one type parameter.");
  // Whatever the facade hands back for this query: a pointer, a tuple of them, or a vector for
  // `Script`. Naming it rather than repeating the three shapes means a default-constructed one is
  // the right empty answer in every case.
  using Result = decltype(app->template GetEntityComponent<T...>(entityId));
  if (!app || !entityId)
    return Result{};
  return app->template GetEntityComponent<T...>(entityId);
}
auto Script::ForEachChild(auto &&func) -> void {
  if (app && entityId)
    app->ForEachChildEntity(entityId, std::forward<decltype(func)>(func));
}
template <typename... T>
auto Script::HasComponent() const -> bool {
  static_assert(sizeof...(T) > 0, "`HasComponent` requires at least one type parameter.");
  if (!app || !entityId)
    return false;
  if (auto scene = app->GetScene(); scene)
    return scene->template EntityHasComponent<T...>(entityId);
  return false;
}
template <typename... T>
auto Script::RemoveComponent() -> bool {
  static_assert(sizeof...(T) > 0, "`RemoveComponent` requires at least one type parameter.");
  if (!app || !entityId)
    return false;
  return app->RemoveEntityComponent<T...>(entityId);
}
} // namespace kuki
