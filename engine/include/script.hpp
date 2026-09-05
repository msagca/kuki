#pragma once
#include <component_type.hpp>
#include <concepts.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <script_state.hpp>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <vector>
namespace kuki {
class Application;
class EntityManager;
class ScriptingSystem;
class KUKI_ENGINE_API Script {
public:
  virtual ~Script() = default;
  auto operator=(const Script &other) -> Script & {
    typeIndex = other.typeIndex;
    return *this;
  }
  EntityID entityId{};
  virtual auto CloneTo(EntityManager &, const EntityID) const -> void = 0;
  virtual auto Display() const -> void;
  /// @brief This script type's name, as the editor lists it and a scene file records it.
  ///
  /// Defaults to the class name that `typeid` reports, with the `class `/`struct ` prefix removed;
  /// override to choose a different one. Overriding is worth doing where the answer has to be
  /// stable, since a scene file stores whatever this returns and `ScriptRegistry::FindByName`
  /// matches on it -- a renamed class would otherwise stop matching scenes saved before the rename.
  ///
  /// Named for the type rather than the script, matching `Asset::GetTypeName`,
  /// `Component::GetTypeName` and the `GetTypeIndex` below. It was `GetName`, which read as the
  /// name of something rather than of a type, and left no clear name for the entity's own -- see
  /// `GetEntityName`.
  virtual auto GetTypeName() const -> std::string;
  virtual auto Start(Application &) -> void;
  virtual auto Update(Application &) -> void;
  virtual auto Shutdown(Application &) -> void;
  template <typename T>
  auto As(this auto &self) -> ConstCorrectPointer<decltype(self), T>;
  auto GetTypeIndex() const -> std::type_index;
  template <typename T>
  auto Is() const -> bool;
protected:
  template <typename T>
  explicit Script(std::in_place_type_t<T>);
  /// @brief The application this script is running in, or null before `Start` and after `Shutdown`.
  ///
  /// Set by `ScriptingSystem` immediately before each call, beside `entityId`, and cleared when the
  /// script is cloned. Every script used to keep this itself -- store the reference `Start` was
  /// handed, null it in `CloneTo`, remember not to touch it too early -- which is the same six
  /// lines in every script and one of them is easy to leave out.
  ///
  /// Prefer the helpers below to reaching through this. It is here for what they do not cover, and
  /// for the callbacks a script registers: an input action outlives the `Update` that registered
  /// it, so it cannot capture the parameter.
  auto GetApp() const -> Application *;
  /// @brief Adds components to the entity this script is attached to.
  ///
  /// The point of these four: a script already knows which entity it is on, so saying so again at
  /// every call is noise. `AddComponent<Light>()` rather than
  /// `app.AddEntityComponent<Light>(entityId)`.
  ///
  /// Deliberately per-script state rather than an ambient "entity currently being scripted" kept
  /// somewhere the scene can see. `entityId` is already a member, set before every call, so these
  /// are correct by construction: there is no window in which the value is stale, nothing to save
  /// and restore if one script's `Update` causes another to run, and no way to call them from a
  /// context where the answer would be meaningless -- they are protected, so only a script can.
  /// An ambient value would fail by silently naming the wrong entity, which is the failure mode
  /// worth designing out rather than documenting.
  ///
  /// A null return means the script is not attached or is not running yet, which is the same thing
  /// `Application::AddEntityComponent` says when there is no scene.
  template <typename... T>
  auto AddComponent() -> decltype(auto);
  template <typename... T>
  auto GetComponent() -> decltype(auto);
  template <typename... T>
  auto HasComponent() const -> bool;
  template <typename... T>
  auto RemoveComponent() -> bool;
  /// @brief Flags this entity's transform for recomputation. See `Application::MarkTransformDirty`.
  ///
  /// Worth having on hand because `Transform` carries no dirty bit: a script that writes a position
  /// and does not call this has written a value nothing will ever apply.
  auto MarkDirty() -> void;
  /// @brief The rest of what a script can ask about the entity it is attached to.
  ///
  /// These exist for one reason: each removes an `entityId` the script would otherwise pass to say
  /// something it already knows. That is the whole test for belonging here, and it is why the list
  /// stops where it does rather than mirroring the facade.
  ///
  /// `Application` has seventy-one public methods and fifteen take an entity; the other fifty-six
  /// -- `DeltaTime`, `GetMousePosition`, `IsBindingHeld`, `RegisterInputAction` and the like -- are
  /// about the application rather than about any entity, so a forwarder for one of those would save
  /// no argument and add a third layer to keep in step with the facade it copies. A script is
  /// handed an `Application &` precisely so it can call those directly, and `app.DeltaTime()` is
  /// already shorter than a helper would be.
  ///
  /// Two of the fifteen are deliberately left out, and both are reachable through `GetApp()` by
  /// anyone who means it.
  ///
  /// `DeleteEntity`, because a script deleting the entity it is running on destroys itself part way
  /// through its own `Update`, and every line after the call is reading freed memory. Spelling it
  /// out is not a defence, but it does make it a decision rather than a convenience.
  ///
  /// And `GetEntityComponent(EntityID, ComponentType)` -- the runtime-typed one that returns a
  /// `ComponentVariant` -- because declaring it here means including `component.hpp`, which names
  /// every component type by value and takes this header's include closure from 6 files to 50, 12
  /// of them backend headers. That is a steep price on a header every script includes, to serve
  /// reflection over a component whose type is not known until run time, which is the editor
  /// inspector's problem and not usually a game's. `GetComponentTypes` below covers the half of
  /// that question a script is likely to ask.
  auto IsAttached() const -> bool;
  auto GetEntityName() const -> std::string;
  auto GetParent() const -> EntityID;
  auto HasChildren() const -> bool;
  auto AddChild(const EntityID) -> bool;
  auto Rename(std::string) -> bool;
  /// @brief Makes this entity's camera the one the renderer draws from.
  auto SetActiveCamera() -> bool;
  /// @brief Moves the active camera to frame this entity.
  auto AlignView() -> void;
  /// @brief Runs a callable for each direct child of this entity, taking its `EntityID`.
  ///
  /// Children only, not descendants, which is what `Application::ForEachChildEntity` does.
  auto ForEachChild(auto &&) -> void;
  /// @brief Which component types this entity currently carries.
  auto GetComponentTypes() const -> std::vector<ComponentType>;
  /// @brief Rebuilds the derived links between this entity's subtree and a model asset.
  ///
  /// Niche, and here for completeness rather than because a game is likely to want it: what needs
  /// it is a subtree that was created from a model and then reloaded, which is why the scene
  /// serializer calls it for an `Animator` on load. A script attached to a model's root can ask for
  /// the same rebuild.
  auto ResolveModel(const AssetID) -> void;
private:
  friend class ScriptingSystem;
  std::type_index typeIndex;
  ScriptState state{ScriptState::Idle};
  Application *app{};
};
template <typename T>
Script::Script(std::in_place_type_t<T>)
  : typeIndex(typeid(T)) {}
template <typename T>
auto Script::As(this auto &self) -> ConstCorrectPointer<decltype(self), T> {
  if (self.template Is<T>())
    return static_cast<ConstCorrectPointer<decltype(self), T>>(&self);
  return nullptr;
}
template <typename T>
auto Script::Is() const -> bool {
  return typeid(T) == typeIndex;
}
} // namespace kuki
