#pragma once
#include <application.hpp>
#include <kuki_engine_export.h>
#include <scene.hpp>
#include <string>
#include <vector>
struct GLFWwindow;
namespace kuki {
/// @brief What the engine's own code and the editor reach in an application, and a game does not.
///
/// The other half of `Application`, in a header that is not installed: a game built against a
/// release cannot include it, so nothing here is a promise to one, and it can change as the engine
/// and the editor need. What used to be here was public on `Application` -- a hundred and more
/// methods, of which the games called a third -- and so every one of them looked like something a
/// game was meant to call, the window and the play state as much as the bindings.
///
/// A view rather than an owner, made where it is used: `Internal(app).Assets()`. One made from a
/// const application is const, and offers only what reads.
///
/// The managers are handed out whole rather than forwarded a method at a time, since the code
/// that reaches past the facade at all -- the renderer, the scene loader, the editor's panels --
/// wants a manager's whole surface, and a forward per method was most of what made the facade long.
class KUKI_ENGINE_API ApplicationInternal {
public:
  explicit ApplicationInternal(Application &app)
    : app(app) {}
  auto Assets() -> AssetManager & {
    return app.assetManager;
  }
  auto Assets() const -> const AssetManager & {
    return app.assetManager;
  }
  auto Input() -> InputManager & {
    return app.inputManager;
  }
  auto Input() const -> const InputManager & {
    return app.inputManager;
  }
  auto Scenes() -> SceneManager & {
    return app.sceneManager;
  }
  auto GetWindow() const -> GLFWwindow * {
    return app.window;
  }
  /// @brief The live presentation surface, or null before startup has created it.
  ///
  /// A backend downcasts this to reach its own device -- `DXRenderer::GetContext` does -- and nothing
  /// else should. Behind this door rather than on the facade for that reason: a game that held it
  /// could downcast to `DXContext` and drive the command list itself, and would compile and appear
  /// to work. A game asking how big the picture is has `Application::GetSurfaceSize`.
  auto GetGraphicsContext() const -> GraphicsContext * {
    return app.graphicsContext.get();
  }
  auto GetRenderingSystem() const -> RenderingSystem * {
    return app.GetRenderingSystem();
  }
  auto GetDescription() const -> const ApplicationDescription & {
    return app.desc;
  }
  /// @brief The archetypes every scene of this application is given when it is made.
  ///
  /// The engine's own, unless a `SystemApplication` was given a `World` that adds to them.
  auto GetArchetypes() const -> const std::vector<ArchetypeSpec> & {
    return app.archetypes;
  }
  /// @brief What the editor has selected; nothing in a game. The renderer outlines it.
  auto GetSelectedEntity() const -> EntityID {
    return app.GetSelectedEntity();
  }
  auto GetSelectedEntities() const -> std::vector<EntityID> {
    return app.GetSelectedEntities();
  }
  /// @brief Changes whether the game is running.
  ///
  /// Leaving `Stopped` begins an input session and starts the tick count again from nought;
  /// arriving at it ends the session, which undoes whatever the game bound and registered. See
  /// `InputManager::BeginSession`. The scene is the caller's to save and put back.
  auto SetPlayState(const PlayState state) -> void {
    app.SetPlayState(state);
  }
  /// @brief Asks for exactly one tick to be run on the next frame, for stepping while paused.
  auto StepTick() -> void {
    app.stepRequested = true;
  }
  /// @brief Makes every frame stand for exactly one tick, however long it really took.
  ///
  /// For a run that is read back frame by frame, as `RECORD_FLAG` reads it: the readback stalls the
  /// GPU, so frames come out slower than the display's, and a clock that kept to the wall would run
  /// ticks two and three to a frame and the video would stutter. Stepped this way, the video plays at
  /// the game's speed however slowly it was made, and the same state and input give the same video,
  /// which is what lets a before and an after be compared. The frame's time is the tick's too, so
  /// whatever runs in `Update` -- particles, wind, a camera that eases -- keeps to it.
  ///
  /// The tick scale is ignored meanwhile: it is how a networked game keeps to another machine's
  /// pace, and nothing recorded offline has one.
  auto SetFixedStep(const bool value) -> void {
    app.fixedStep = value;
  }
  /// @brief Makes this application a host: a request to quit stops the game instead of the application.
  ///
  /// A game is ended by the settings menu's Quit. Hosted, that would close the editor around it, so
  /// while a game is playing the request is kept for the host to collect with `TakeStopRequest` and
  /// act on. Stopped, `Quit` still means quit.
  auto SetHosting(const bool value) -> void {
    app.hosting = value;
  }
  auto TakeStopRequest() -> bool {
    return std::exchange(app.stopRequested, false);
  }
  /// @brief Asks the loop to stop after the current frame.
  ///
  /// The polite half of what the window's close button does: `Status` is a question rather than an
  /// instruction, and closing the window was previously the sole route to a false answer. Sets the
  /// same flag the close callback sets, so shutdown runs exactly as it does when someone closes the
  /// window.
  auto Quit() -> void {
    app.Quit();
  }
  /// @brief What `Run` returns, and so what the process exits with. For a run made from the command
  /// line that has an answer to give besides what it logs.
  auto SetExitCode(const int code) -> void {
    app.exitCode = code;
  }
  /// @brief How fast the tick clock runs against the wall clock, from just above nothing up to one.
  ///
  /// For a game played against another machine, which has to keep to the other's pace: one that
  /// runs ahead can only wait for the other to catch up, and waiting is felt as a stall. Running
  /// a little slow until the other has caught up is not. It does not change what a tick is, only
  /// how often one is due, so the rules and everything counted in ticks are unaffected. Never above
  /// one: a clock that ran fast would be a way to get further ahead of the other.
  auto GetTickScale() const -> double {
    return app.tickScale;
  }
  auto SetTickScale(const double scale) -> void;
  /// @brief Set by the network system when it starts and cleared when it shuts down. See
  /// `Application::GetNetwork`.
  auto SetNetwork(NetworkSystem *value) -> void {
    app.network = value;
  }
  /// @brief Whether the pointer is over the editor's viewport, which its camera controls read.
  auto IsViewportHovered() const -> bool {
    return app.viewportHovered;
  }
  auto SetViewportHovered(const bool hovered) -> void {
    app.viewportHovered = hovered;
  }
  auto SetCursorLocked(const bool) -> void;
  /// @brief Registers a binding whose default may carry modifiers, as the editor's shortcuts do.
  /// On a rebound key as `Application::RegisterBinding` is.
  auto RegisterBinding(const std::string &name, const InputManager::Trigger &trigger, std::string description = "") -> void {
    app.RegisterTrigger(name, trigger, std::move(description));
  }
  /// @brief The id of the overlay text under a point in window coordinates, or `Overlay::NoHit`.
  ///
  /// The overlay's counterpart of `PickEntity`, and there for the same reason that one is: the
  /// overlay is laid out in the render target's pixels and the mouse is reported in the window's,
  /// and the two are only the same size by coincidence.
  auto PickOverlay(const glm::vec2 &position) -> int {
    return app.PickOverlay(position);
  }
  auto CreateScene(std::string name = "") -> SceneID;
  /// @brief A scene by name, or the active one. Null when there is none. A game has no scene to
  /// hold: what it reads of one is `Application::GetCamera` and `GetSceneSingleton`.
  auto GetScene(const std::string &name = "") -> Scene * {
    return app.sceneManager.Get(name);
  }
  auto GetScene(const std::string &name = "") const -> const Scene * {
    return std::as_const(app.sceneManager).Get(name);
  }
  /// @brief Makes an existing scene the active one. False when nothing goes by that name.
  auto SwitchScene(const std::string &name) -> bool {
    return app.sceneManager.Switch(name);
  }
  /// @brief The unchecked creation `World` checks, for the builder and the scene loader, which
  /// have a mask and not a type. See `Application::CreateEntity`.
  auto CreateEntity(const ComponentMask &mask, std::string name = "") -> EntityID {
    return app.CreateEntity(mask, std::move(name));
  }
  auto CreateEntity(const ComponentMask &mask, const PoolKey &pool, std::string name = "") -> EntityID {
    return app.CreateEntity(mask, pool, std::move(name));
  }
  auto CreatePool(const ComponentMask &mask, const PoolKey &pool, PoolSettings settings = {}) -> bool {
    return app.CreatePool(mask, pool, std::move(settings));
  }
  auto InstantiateAsset(const AssetID id, const ComponentMask &root) -> EntityID {
    return app.InstantiateAsset(id, root);
  }
  auto InstantiateAsset(const std::string &name, const ComponentMask &root) -> EntityID {
    return app.InstantiateAsset(name, root);
  }
  /// @brief Rebinds a loaded model's entities to the model's meshes and nodes. See
  /// `AssetManager::ResolveModelInstance`.
  auto ResolveModelInstance(const EntityID, const AssetID) -> void;
  /// @brief The active scene's entities, every archetype at once, unchecked.
  ///
  /// For the engine's own systems, which serve whatever world they are listed in and so cannot
  /// name it; a game's queries go through its `World`, which refuses one that matches nothing.
  /// These and the forwards below do nothing, or answer nothing, when no scene is active.
  template <typename... T>
  auto ForEachEntity(this auto &&self, auto &&func) -> void {
    if (auto scene = self.ActiveScene(); scene)
      scene->Entities().template ForEach<T...>(std::forward<decltype(func)>(func));
  }
  auto ForEachRootEntity(this auto &&self, auto &&func) -> void {
    if (auto scene = self.ActiveScene(); scene)
      scene->Entities().ForEachRoot(std::forward<decltype(func)>(func));
  }
  auto ForEachChildEntity(this auto &&self, const EntityID parent, auto &&func) -> void {
    if (auto scene = self.ActiveScene(); scene)
      scene->Entities().ForEachChild(parent, std::forward<decltype(func)>(func));
  }
  auto GetEntityName(const EntityID id) const -> std::string {
    const auto scene = ActiveScene();
    return scene ? scene->Entities().GetName(id) : "";
  }
  auto RenameEntity(const EntityID id, std::string name) -> bool {
    auto scene = ActiveScene();
    return scene && scene->Entities().Rename(id, std::move(name));
  }
  auto EntityHasChildren(const EntityID id) const -> bool {
    const auto scene = ActiveScene();
    return scene && scene->Entities().HasChildren(id);
  }
  auto GetEntitySignature(const EntityID id) const -> ComponentMask {
    const auto scene = ActiveScene();
    return scene ? scene->Entities().GetSignature(id) : ComponentMask{};
  }
  auto GetSavedComponent(const EntityID id, const ComponentId component) -> SavedComponent {
    auto scene = ActiveScene();
    return scene ? scene->Entities().GetSavedComponent(id, component) : SavedComponent{};
  }
  auto GetEntityComponentTypes(const EntityID id) const -> std::vector<ComponentType> {
    const auto scene = ActiveScene();
    return scene ? scene->Entities().GetComponentTypes(id) : std::vector<ComponentType>{};
  }
  auto GetEntityComponent(const EntityID id, const ComponentType type) -> std::optional<ComponentVariant> {
    auto scene = ActiveScene();
    if (!scene)
      return std::nullopt;
    return scene->Entities().GetComponent(id, type);
  }
  auto GetEntityCount() const -> size_t {
    const auto scene = ActiveScene();
    return scene ? scene->Entities().GetCount() : 0;
  }
  /// @brief An entity's bounds around its origin, its own or its pool's. See `EntityManager::GetBounds`.
  auto GetEntityBounds(const EntityID id) const -> BoundingBox {
    const auto scene = ActiveScene();
    return scene ? scene->Entities().GetBounds(id) : BoundingBox{};
  }
  auto DeleteEntities() -> void {
    if (auto scene = ActiveScene(); scene)
      scene->Clear();
  }
  auto SetActiveCamera(const EntityID id) -> bool {
    auto scene = ActiveScene();
    return scene && scene->SetActiveCamera(id);
  }
private:
  Application &app;
  auto ActiveScene() -> Scene * {
    return app.sceneManager.Get();
  }
  auto ActiveScene() const -> const Scene * {
    return std::as_const(app.sceneManager).Get();
  }
};
inline auto Internal(Application &app) -> ApplicationInternal {
  return ApplicationInternal(app);
}
/// @brief Const, so only what reads can be called on it. The cast is never written through: the
/// const view offers nothing that writes.
inline auto Internal(const Application &app) -> const ApplicationInternal {
  return ApplicationInternal(const_cast<Application &>(app));
}
} // namespace kuki
