#pragma once
#include <application_description.hpp>
#include <archetype.hpp>
#include <asset_manager.hpp>
#include <component_type.hpp>
#include <concepts.hpp>
#include <engine_archetypes.hpp>
#include <entity_manager.hpp>
#include <graphics_context.hpp>
#include <id.hpp>
#include <input_manager.hpp>
#include <key.hpp>
#include <kuki_engine_export.h>
#include <memory>
#include <optional>
#include <overlay.hpp>
#include <preferences.hpp>
#include <primitive.hpp>
#include <rig.hpp>
#include <scene_manager.hpp>
#include <settings_menu.hpp>
#include <shader_asset.hpp>
#include <string_view>
#include <system.hpp>
#include <tick_clock.hpp>
#include <utility>
#include <vector>
namespace kuki {
class ApplicationInternal;
class NetworkSystem;
class RenderingSystem;
/// @brief Whether the game in an application is running, as opposed to the application.
///
/// A game on its own is `Playing` from start to finish and never thinks about this. An editor is
/// the application that does: it starts `Stopped`, with the scene standing still to be worked on,
/// and plays, pauses and stops the game inside it.
///
/// Stopped and paused both mean no tick is run. What tells them
/// apart is what the host does about it: stopping puts the scene back as it was before play.
enum class PlayState : uint8_t { Stopped,
  Playing,
  Paused };
/// @brief What a game is given: its time, its input, its assets, its scene, and the few services
/// every game has.
///
/// Kept to what a game calls, and that is a rule rather than a tidying. Everything here is in the
/// installed headers and so is a promise to every game built against a release; what only the
/// engine's own code and the editor reach -- the managers themselves, the window, play states, the
/// editor's selection -- is `ApplicationInternal`'s, in a header that is not installed. A method a
/// game has no use for was a second way to do something, or a way past a rule the engine keeps:
/// raw keys past the bindings (every action is a named binding, rebindable), callbacks past the
/// polling (`RegisterInputAction`), an unchecked archetype past the `World` that checks it.
///
/// Entities are made and visited through the game's `World` (`World::Create`, `World::ForEach`,
/// the builder from `World::Game`), which refuses at compile time an archetype the game never
/// declared; the unchecked versions underneath are private, and `World` is let in.
class KUKI_ENGINE_API Application {
public:
  Application(ApplicationDescription = {});
  virtual ~Application();
  /// @brief Brings the application up, runs frames until `Status` says stop, and takes it down.
  ///
  /// The one lifecycle entry point that is public, because `main` has to call something. The four
  /// hooks it drives are protected -- see the note on `Start`.
  auto Run() -> void;
  /// @brief How long the frame being updated took, in seconds. For presentation; rules count ticks.
  auto DeltaTime() const -> float;
  /// @brief Whether the game is running, paused by the settings menu, or (in an editor) stopped.
  auto GetPlayState() const -> PlayState;
  /// @brief The fixed step: how far through the next tick drawing is, which tick is running, how many
  /// make a second and how long one lasts.
  ///
  /// `System::Tick` is called a whole number of times each frame, for as many
  /// steps of `TickTime` seconds as the frame's time has made due. See `TickClock` for why, and for
  /// what `TickAlpha` is for.
  ///
  /// The rate is sixty until something sets it, and is meant to be set once, before the game is
  /// under way: everything counted in ticks -- a gesture's window, a move's length -- changes
  /// duration when it changes.
  auto TickAlpha() const -> float;
  auto TickIndex() const -> uint64_t;
  auto TickRate() const -> int;
  auto TickTime() const -> float;
  auto SetTickRate(const int) -> void;
  /// @brief Registers a named binding, on the key the player last rebound it to if they did.
  ///
  /// The key given is the default, which the settings menu's reset puts back. A rebinding is kept in
  /// the preferences under `input.` and the name; see `SettingsMenu::SaveBindings`. A default is a
  /// key alone: the player may rebind it to a key with modifiers, but a game does not start one
  /// there, since a chord is the editor's kind of shortcut and not a game's kind of action.
  auto RegisterBinding(const std::string &, const Key, std::string = "") -> void;
  /// @brief Registers a binding that is also one of the game's buttons, on a rebound key as
  /// `RegisterBinding` is. See `InputManager::RegisterButton`.
  auto RegisterButton(const std::string &, const Key, std::string = "") -> int;
  auto IsBindingHeld(const std::string &) const -> bool;
  auto IsBindingPressed(const std::string &) const -> bool;
  /// @brief Which registered buttons are down for the tick being run. See `InputManager::GetButtons`.
  auto GetButtons() const -> uint32_t;
  /// @brief Whether a mouse button is down now, went down this frame, or came up this frame.
  ///
  /// Read directly, since a mouse button cannot be rebound. There is no counterpart for keys: a key
  /// is read through the binding it is the default of, which is what lets the player move it.
  auto IsMouseHeld(const MouseButton) const -> bool;
  auto IsMousePressed(const MouseButton) const -> bool;
  auto IsMouseReleased(const MouseButton) const -> bool;
  auto GetMousePosition() const -> glm::vec2;
  auto GetScrollOffset() const -> glm::vec2;
  /// @brief The window's size in the units `GetMousePosition` reports the cursor in, which is what a
  /// cursor position is divided by to say where on the picture it is. Zeroes before there is a window.
  auto GetWindowSize() const -> glm::ivec2;
  /// @brief The size in pixels of what is presented to, which the overlay is laid out in. Zeroes
  /// before there is a surface. Not the window's size: on a scaled display the two differ.
  auto GetSurfaceSize() const -> std::pair<int, int>;
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
  template <IsAsset T>
  auto LoadAsset(const std::filesystem::path &, std::string = "", const AssetID = {}) -> AssetID;
  auto GetAsset(this auto &self, const AssetID) -> ConstCorrectPointer<decltype(self), Asset>;
  auto GetAsset(this auto &self, const std::string &) -> ConstCorrectPointer<decltype(self), Asset>;
  template <IsAsset T>
  auto GetAsset(const AssetID) -> decltype(auto);
  /// @brief The camera the active scene is drawn from: the one the builder made active, or else the
  /// first there is. Null when there is none.
  auto GetCamera() -> Camera *;
  auto GetCamera() const -> const Camera *;
  /// @brief One of the active scene's singletons -- `Wind`, `ScreenEffects`, `AtmosphereSky` and
  /// the rest of what `Component::IsSceneSingleton` names -- or null when the scene has none.
  ///
  /// The whole of what a game wanted a scene for, besides its camera. A component that is not a
  /// singleton is refused at compile time: there may be many, and "any one of them" is not an
  /// answer a game can use. An entity's own component is `GetEntityComponent`, and many are
  /// visited with `World::ForEach`.
  template <IsComponent T>
  auto GetSceneSingleton(this auto &self) -> ConstCorrectPointer<decltype(self), T>;
  template <typename... T>
  auto GetEntityComponent(this auto &, const EntityID) -> decltype(auto);
  /// @brief Flags an entity's transform, and everything below it, for recomputation.
  ///
  /// Call after writing to a transform's position, rotation or scale; the component carries no
  /// dirty bit of its own, so an unannounced change goes unapplied.
  auto MarkTransformDirty(const EntityID) -> void;
  auto IsEntity(const EntityID) const -> bool;
  auto DeleteEntity(const EntityID) -> void;
  auto AddChildEntity(const EntityID, const EntityID) -> bool;
  auto GetEntityParent(const EntityID) const -> EntityID;
  /// @brief Gives an entity of a drawn archetype another mesh or material by moving it to that
  /// pool. See `EntityManager::MoveToPool`.
  auto MoveEntityToPool(const EntityID, const PoolKey &) -> bool;
  auto GetEntityPool(const EntityID) const -> PoolKey;
  /// @brief The rig of a model's root in the active scene. See `Scene::GetRig`.
  auto GetRig(const EntityID) -> Rig *;
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
  /// @brief The network system, if this application has one, and null if it does not.
  ///
  /// Set by the system when it starts and cleared when it shuts down. A game that wants to be played
  /// over a network asks for it and falls back on running its own rules when there is none, which is
  /// the case in an editor that hosts the game without listing the system.
  auto GetNetwork() const -> NetworkSystem *;
  /// @brief Named values this application keeps between runs. See `Preferences`.
  ///
  /// Loaded before `Start` and written back after the systems have shut down, so a system may read
  /// them as it starts and set them as it stops without arranging either itself.
  auto GetPreferences(this auto &self) -> decltype(auto) {
    return (self.preferences);
  }
  /// @brief The settings every game has, which a game adds its own page of entries to. See
  /// `SettingsMenu`.
  auto GetSettingsMenu(this auto &self) -> decltype(auto) {
    return (self.settingsMenu);
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
  /// @brief Renames the window.
  ///
  /// Worth having for its own sake, and worth more than usual here: with no text rendering in the
  /// engine, the title bar is the only place a game can put a line of status.
  auto SetWindowTitle(const std::string &) -> void;
protected:
  /// @brief The hooks `Run` drives the application through, for a subclass to fill in.
  ///
  /// Protected rather than public, because nothing holding an `Application` has any business
  /// calling them -- and every system holds one. A system is handed the whole facade by design, so
  /// while these were public one could call `Shutdown` mid-frame or `Update` a second time within
  /// the frame it was called from. Neither is a thing anyone did; both were a thing the type
  /// permitted, and `Run` is the only caller either ever had.
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
  /// @brief Acts on the settings menu's input and queues it into the overlay, when it is enabled.
  ///
  /// For `SystemApplication::UpdateSystems` to call after every system has updated and late-updated
  /// and before anything is drawn, so the menu lies over whatever the systems queued.
  ///
  /// This and `SetArchetypes` are protected rather than private with `SystemApplication` a friend,
  /// because befriending it would mean naming its constraint, `IsWorld`, which `world.hpp` declares
  /// after including this header.
  auto UpdateSettingsMenu(const float) -> void;
  /// @brief Replaces the archetypes scenes are made with. For `SystemApplication`, before any scene is.
  auto SetArchetypes(std::vector<ArchetypeSpec>) -> void;
private:
  /// @brief The engine's own code and the editor, through the one door. See `ApplicationInternal`.
  friend class ApplicationInternal;
  /// @brief The menu reaches past the facade: it takes the keyboard, lists and rebinds every binding,
  /// switches the window to fullscreen and sets the renderer's passes -- each a thing the facade
  /// keeps from a game on purpose, and each the whole of what a settings screen is for.
  friend class SettingsMenu;
  /// @brief For the unchecked creation below, which `World` checks before it calls.
  template <IsArchetype...>
  friend struct World;
  GLFWwindow *window{};
  std::unique_ptr<GraphicsContext> graphicsContext;
  ApplicationDescription desc;
  AssetManager assetManager;
  Overlay overlay;
  Preferences preferences;
  InputManager inputManager;
  SceneManager sceneManager;
  SettingsMenu settingsMenu;
  std::vector<ArchetypeSpec> archetypes;
  float deltaTime{};
  TickClock tickClock;
  double tickScale{1.};
  NetworkSystem *network{};
  PlayState playState{PlayState::Playing};
  bool hosting{};
  bool stopRequested{};
  bool stepRequested{};
  bool viewportHovered{};
  /// @brief Private and virtual: `SystemApplication` and `Editor` say what they are, and only the
  /// engine asks, through `ApplicationInternal`. Overriding does not need access.
  virtual auto GetRenderingSystem() -> RenderingSystem * {
    return nullptr;
  }
  virtual auto GetSelectedEntity() const -> EntityID {
    return {};
  }
  virtual auto GetSelectedEntities() const -> std::vector<EntityID> {
    const auto id = GetSelectedEntity();
    if (!id)
      return {};
    return {id};
  }
  virtual auto StartSystems() -> void {}
  virtual auto PostStartSystems() -> void {}
  virtual auto UpdateSystems(const float) -> void {}
  virtual auto TickSystems() -> void {}
  virtual auto ShutdownSystems() -> void {}
  /// @brief Makes an entity of an archetype, given as its mask, in the active scene, in the pool of
  /// a mesh and a material when one is given, making the pool if it is the first. A pool made this
  /// way is given the mesh's bounds; one made with `CreatePool` first keeps what it was given.
  ///
  /// Unchecked: any mask is taken. `World::Create` is the checked route, and the builder and the
  /// scene loader, which have only a mask, reach these through `ApplicationInternal`.
  auto CreateEntity(const ComponentMask &, std::string = "") -> EntityID;
  auto CreateEntity(const ComponentMask &, const PoolKey &, std::string = "") -> EntityID;
  /// @brief Makes a pool of a drawn archetype in the active scene ahead of its entities. Bounds left
  /// unset are the mesh's. See `PoolSettings`.
  auto CreatePool(const ComponentMask &, const PoolKey &, PoolSettings = {}) -> bool;
  /// @brief Places an asset in the active scene: a model, as an entity of the root's archetype with
  /// the model's nodes below it, or a mesh as a `MeshObject`. See `AssetManager::Instantiate`.
  auto InstantiateAsset(const AssetID, const ComponentMask &) -> EntityID;
  auto InstantiateAsset(const std::string &, const ComponentMask &) -> EntityID;
  /// @brief The active scene's entities, or null when no scene is active. Out of line, so that this
  /// header need not include `scene.hpp`, which is internal; what a game reaches of the entities is
  /// what the templates above and `World` take from here.
  auto ActiveEntities() -> EntityManager *;
  auto ActiveEntities() const -> const EntityManager *;
  /// @brief The bounds of what a pool key draws: its mesh's, or empty if the mesh is not loaded.
  auto GetMeshBounds(const PoolKey &) -> BoundingBox;
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
  /// @brief Asks the loop to stop after the current frame, or the host to stop the game. See
  /// `ApplicationInternal::Quit`.
  auto Quit() -> void;
  /// @brief Changes whether the game is running. See `ApplicationInternal::SetPlayState`.
  auto SetPlayState(const PlayState) -> void;
  /// @brief Takes the window to borderless fullscreen on the primary monitor, or back to a window.
  auto SetFullscreen(const bool) -> void;
  /// @brief Registers a binding on a trigger that may carry modifiers, on the saved one if there
  /// is one. Behind `RegisterBinding` here and in `ApplicationInternal`, which the editor's chorded
  /// shortcuts go through.
  auto RegisterTrigger(const std::string &, const InputManager::Trigger &, std::string) -> void;
  /// @brief The trigger a binding was last saved on, if it was.
  auto SavedBinding(const std::string &) const -> std::optional<InputManager::Trigger>;
  /// @brief The overlay text under a point in window coordinates. See `ApplicationInternal::PickOverlay`.
  auto PickOverlay(const glm::vec2 &) -> int;
  static auto CharCallback(GLFWwindow *, unsigned int) -> void;
  static auto CursorPosCallback(GLFWwindow *, double, double) -> void;
  static auto FramebufferSizeCallback(GLFWwindow *, int, int) -> void;
  static auto KeyCallback(GLFWwindow *, int, int, int, int) -> void;
  static auto MouseButtonCallback(GLFWwindow *, int, int, int) -> void;
  static auto ScrollCallback(GLFWwindow *, double, double) -> void;
  static auto WindowCloseCallback(GLFWwindow *) -> void;
};
auto Application::GetAsset(this auto &self, const AssetID id) -> ConstCorrectPointer<decltype(self), Asset> {
  return self.assetManager.Get(id);
}
auto Application::GetAsset(this auto &self, const std::string &name) -> ConstCorrectPointer<decltype(self), Asset> {
  return self.assetManager.Get(name);
}
template <IsComponent T>
auto Application::GetSceneSingleton(this auto &self) -> ConstCorrectPointer<decltype(self), T> {
  static_assert(Component::IsSceneSingleton(static_cast<ComponentType>(ComponentIdOf<T>)), "GetSceneSingleton: that component is not a scene singleton, so a scene may hold many -- read an entity's with GetEntityComponent, or visit them all with World::ForEach");
  auto entities = self.ActiveEntities();
  return entities ? entities->template GetAny<T>() : nullptr;
}
template <IsAsset T>
auto Application::GetAsset(const AssetID id) -> decltype(auto) {
  return assetManager.Get<T>(id);
}
template <typename... T>
auto Application::GetEntityComponent(this auto &self, const EntityID id) -> decltype(auto) {
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    if (auto entities = self.ActiveEntities(); entities)
      return entities->template GetComponent<T...>(id);
    else
      return static_cast<ConstCorrectPointer<decltype(self), C>>(nullptr);
  } else {
    if (auto entities = self.ActiveEntities(); entities)
      return entities->template GetComponent<T...>(id);
    else
      return std::tuple(self.template GetEntityComponent<T>(id)...);
  }
}
template <IsAsset T>
auto Application::LoadAsset(const std::filesystem::path &path, std::string name, const AssetID forcedId) -> AssetID {
  return assetManager.Load<T>(path, std::move(name), forcedId);
}
} // namespace kuki
