#pragma once
#include <application.hpp>
#include <archetype.hpp>
#include <camera.hpp>
#include <component_traits.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <material_handle.hpp>
#include <memory>
#include <mesh_handle.hpp>
#include <skybox_handle.hpp>
#include <string>
#include <transform.hpp>
#include <utility>
#include <vector>
namespace kuki {
/// @brief Which of the three nesting levels a builder call belongs to.
enum class BuilderScope : uint8_t { Game,
  Scene,
  Entity };
/// @brief The nesting the fluent builders share, so that a call can find the level it belongs to.
///
/// One of these is created per `Game` and held by every builder in the chain through a
/// `shared_ptr`, which is what lets each call return a builder by value without copying state.
///
/// The stack is the whole trick. A call names the level it needs rather than the level it is at,
/// and `PopTo` unwinds until it gets there -- so `.With<Light>(...).Entity<MeshObject>("Pawn")`
/// works without a matching `End()`, because `Entity` asks for scene scope and the entity frame
/// above it is discarded on the way. Descending is explicit (`Child`) and so is coming back up
/// (`Parent`), because those are the two cases the level of a call cannot disambiguate: at entity
/// scope a second `Child` is a grandchild, never a sibling. That is the limitation of doing without
/// paired delimiters, and it is why anything with real hierarchy is better off calling
/// `CreateEntity` and `AddChildEntity` directly.
class BuilderStack {
public:
  explicit BuilderStack(Application &app)
    : app(app) {
    frames.push_back({});
  }
  struct Frame {
    BuilderScope scope{BuilderScope::Game};
    EntityID entity{};
  };
  Application &app;
  /// @brief Discards frames until the innermost one is at this scope. The game frame never pops.
  auto PopTo(const BuilderScope scope) -> void {
    while (frames.size() > 1 && frames.back().scope != scope)
      frames.pop_back();
  }
  auto Push(const BuilderScope scope, const EntityID entity = {}) -> void {
    frames.push_back({scope, entity});
  }
  auto Pop() -> void {
    if (frames.size() > 1)
      frames.pop_back();
  }
  auto Scope() const -> BuilderScope {
    return frames.back().scope;
  }
  /// @brief The entity calls apply to, or an invalid id outside entity scope.
  auto Current() const -> EntityID {
    return frames.back().entity;
  }
private:
  std::vector<Frame> frames;
};
/// @brief A world that checks nothing, for a builder started from `Game` with no world named.
///
/// The archetypes are still checked against each entity's own -- `With<Light>` on an entity made
/// without a light does not compile -- but not against the game's: a builder with no world cannot
/// know which archetypes the game declared, and finds out at run time, when an undeclared one makes
/// no entity and says so in the log. A game names its world to get that checked too.
struct AnyWorld {
  template <typename A>
  static constexpr bool Has = true;
};
/// @brief The parts of the builders that are the same whatever the archetype, compiled once.
namespace builder {
  /// @brief Resolves an asset name, complaining once rather than leaving a silent empty handle.
  KUKI_ENGINE_API auto ResolveAsset(Application &, const std::string &) -> AssetID;
  KUKI_ENGINE_API auto BeginGame(Application &, const std::string &) -> void;
  KUKI_ENGINE_API auto BeginScene(BuilderStack &, std::string) -> void;
  /// @brief Makes a root entity of an archetype and makes it the one calls apply to.
  KUKI_ENGINE_API auto BeginEntity(BuilderStack &, const ComponentMask &, std::string) -> void;
  /// @brief Makes an existing entity the one calls apply to, if it has every component of `mask`.
  KUKI_ENGINE_API auto ReopenEntity(BuilderStack &, const ComponentMask &, const EntityID) -> void;
  /// @brief Makes an entity of an archetype under the current one and descends into it.
  KUKI_ENGINE_API auto BeginChild(BuilderStack &, const ComponentMask &, std::string) -> void;
  KUKI_ENGINE_API auto MarkMoved(BuilderStack &) -> void;
} // namespace builder
template <typename W>
class SceneBuilder;
/// @brief Fluent builder for one entity of archetype `A`, and the level every component call lands on.
///
/// Returned by value throughout. It holds a `shared_ptr` and nothing else, so a chain of these
/// costs one allocation for the whole `Game(...)` expression rather than one per link.
///
/// Every call is checked against `A` at compile time: `Mesh` needs a `MeshHandle` in it, `At` a
/// `Transform`, `With<T>` a `T`. Nothing here adds a component, because nothing can: an entity's
/// components are its archetype's, chosen where `Entity<A>` or `Child<A>` made it. `Up` is the
/// builder `Parent` returns to.
///
/// Naming pitfall worth knowing: `Mesh` and `Scene` are member names here and shadow the engine
/// types of those names inside this class.
template <typename W, IsArchetype A, typename Up>
class EntityBuilder {
public:
  explicit EntityBuilder(std::shared_ptr<BuilderStack> stack)
    : stack(std::move(stack)) {}
  /// @brief The entity being built, for keeping hold of it.
  auto Id() const -> EntityID {
    return stack->Current();
  }
  /// @brief Points the entity at a mesh asset by the name the asset manager knows it by.
  ///
  /// A name that names nothing is logged and otherwise ignored, rather than asserting: a scene
  /// described in one expression is worth getting a partial picture of, and a missing mesh shows
  /// up as a missing object rather than as a dead process.
  auto Mesh(const std::string &name) -> EntityBuilder {
    return Mesh(builder::ResolveAsset(stack->app, name));
  }
  auto Mesh(const AssetID asset) -> EntityBuilder {
    static_assert(A::template Has<MeshHandle>, "Mesh: this entity's archetype has no MeshHandle -- make it with `Renderable` in it, as `MeshObject` is");
    if (auto *handle = Get<MeshHandle>(); handle && asset)
      handle->assetId = asset;
    return *this;
  }
  auto Material(const std::string &name) -> EntityBuilder {
    return Material(builder::ResolveAsset(stack->app, name));
  }
  auto Material(const AssetID asset) -> EntityBuilder {
    static_assert(A::template Has<MaterialHandle>, "Material: this entity's archetype has no MaterialHandle -- make it with `Renderable` in it, as `MeshObject` is");
    if (auto *handle = Get<MaterialHandle>(); handle && asset)
      handle->assetId = asset;
    return *this;
  }
  auto Skybox(const std::string &name) -> EntityBuilder {
    static_assert(A::template Has<SkyboxHandle>, "Skybox: this entity's archetype has no SkyboxHandle -- make it as a `SkyboxObject`");
    const auto asset = builder::ResolveAsset(stack->app, name);
    if (auto *handle = Get<SkyboxHandle>(); handle && asset)
      handle->assetId = asset;
    return *this;
  }
  /// @brief Sets the local position, marking the transform so the change is actually applied.
  ///
  /// `Transform` carries no dirty bit, so every mutator here calls `MarkTransformDirty`. Doing
  /// that for you is most of the reason to reach for this over writing the component by hand.
  auto At(const glm::vec3 &position) -> EntityBuilder {
    return Move([&](Transform &transform) { transform.position = position; });
  }
  auto Scale(const glm::vec3 &scale) -> EntityBuilder {
    return Move([&](Transform &transform) { transform.scale = scale; });
  }
  auto Scale(const float scale) -> EntityBuilder {
    return Scale(glm::vec3(scale));
  }
  auto Rotate(const glm::quat &rotation) -> EntityBuilder {
    return Move([&](Transform &transform) { transform.rotation = rotation; });
  }
  /// @brief Sets the rotation from yaw, pitch and roll in degrees.
  auto RotateEuler(const glm::vec3 &degrees) -> EntityBuilder {
    return Rotate(glm::quat(glm::radians(degrees)));
  }
  /// @brief Makes this entity's camera the one the renderer draws from.
  auto ActiveCamera() -> EntityBuilder {
    static_assert(A::template Has<Camera>, "ActiveCamera: this entity's archetype has no Camera -- make it as a `CameraObject`");
    if (const auto id = stack->Current(); id)
      stack->app.SetActiveCamera(id);
    return *this;
  }
  /// @brief Hands one of the entity's components to a callable to fill in.
  ///
  /// The escape hatch that keeps this class from needing a method per component type. Anything
  /// with no named convenience above is reached with `With<Light>([](Light &l) { ... })`.
  template <IsComponent T>
  auto With(auto &&configure) -> EntityBuilder {
    static_assert(A::template Has<T>, "With: this entity's archetype does not have that component, and an entity cannot be given one -- declare an archetype that has it");
    if (auto *component = Get<T>(); component)
      configure(*component);
    return *this;
  }
  /// @brief Makes a child of the current entity, of archetype `B`, and descends into it.
  template <IsArchetype B>
  auto Child(std::string name = "") -> EntityBuilder<W, B, EntityBuilder> {
    static_assert(A::template Has<Transform> && B::template Has<Transform>, "Child: a parent and its child both need a Transform");
    static_assert(W::template Has<B>, "Child: this world does not declare that archetype");
    builder::BeginChild(*stack, B::Mask(), std::move(name));
    return EntityBuilder<W, B, EntityBuilder>(stack);
  }
  /// @brief Comes back up one level, so a sibling child can follow.
  ///
  /// At the top of a scene this leaves scope where it was; the builder returned is then only
  /// useful for `Entity` or `Scene`, both of which pop on their own account anyway.
  auto Parent() -> Up {
    stack->Pop();
    return Up(stack);
  }
  /// @brief Starts a new root entity of archetype `B` in the current scene, abandoning whatever
  /// entity was open.
  template <IsArchetype B>
  auto Entity(std::string name = "") -> EntityBuilder<W, B, SceneBuilder<W>> {
    return SceneBuilder<W>(stack).template Entity<B>(std::move(name));
  }
  /// @brief Re-opens an entity that already exists, so the calls after it edit that one. See
  /// `SceneBuilder::Entity`.
  template <IsArchetype B>
  auto Entity(const EntityID id) -> EntityBuilder<W, B, SceneBuilder<W>> {
    return SceneBuilder<W>(stack).template Entity<B>(id);
  }
  /// @brief Starts a new scene, abandoning whatever entity and scene were open.
  auto Scene(std::string name) -> SceneBuilder<W> {
    return SceneBuilder<W>(stack).Scene(std::move(name));
  }
private:
  std::shared_ptr<BuilderStack> stack;
  template <IsComponent T>
  auto Get() -> T * {
    if (const auto id = stack->Current(); id)
      return stack->app.template GetEntityComponent<T>(id);
    return nullptr;
  }
  auto Move(auto &&change) -> EntityBuilder {
    static_assert(A::template Has<Transform>, "this entity's archetype has no Transform to place it with");
    if (auto *transform = Get<Transform>(); transform) {
      change(*transform);
      builder::MarkMoved(*stack);
    }
    return *this;
  }
};
/// @brief Fluent builder for one scene: somewhere for entities to be created.
template <typename W>
class SceneBuilder {
public:
  explicit SceneBuilder(std::shared_ptr<BuilderStack> stack)
    : stack(std::move(stack)) {}
  /// @brief Makes a root entity of archetype `A`, which the game's world must declare.
  template <IsArchetype A>
  auto Entity(std::string name = "") -> EntityBuilder<W, A, SceneBuilder> {
    static_assert(W::template Has<A>, "Entity: this world does not declare that archetype, so no scene of it can hold one");
    builder::BeginEntity(*stack, A::Mask(), std::move(name));
    return EntityBuilder<W, A, SceneBuilder>(stack);
  }
  /// @brief Re-opens an entity that already exists, so the calls after it edit that one.
  ///
  /// `A` is what the calls after it may touch, and the entity must have every component of it --
  /// it may have more. One that does not, or an id that is no longer an entity, is reported and
  /// edits nothing, rather than crashing.
  ///
  /// The builders otherwise only ever create, which leaves anything that keeps its entities across
  /// a change writing components by hand. Editing in place is worth having a route to: an entity
  /// destroyed and made again is a new id, and picking reads the id buffer the last frame left
  /// behind -- so a scene that rebuilds itself answers a click with an id that no longer resolves.
  template <IsArchetype A>
  auto Entity(const EntityID id) -> EntityBuilder<W, A, SceneBuilder> {
    builder::ReopenEntity(*stack, A::Mask(), id);
    return EntityBuilder<W, A, SceneBuilder>(stack);
  }
  auto Scene(std::string name) -> SceneBuilder {
    builder::BeginScene(*stack, std::move(name));
    return SceneBuilder(stack);
  }
private:
  std::shared_ptr<BuilderStack> stack;
};
/// @brief Root of the fluent builders. Obtained from `Game`, or from a world's `World::Game`.
template <typename W>
class GameBuilder {
public:
  GameBuilder(Application &app, const std::string &name)
    : stack(std::make_shared<BuilderStack>(app)) {
    builder::BeginGame(app, name);
  }
  auto Scene(std::string name) -> SceneBuilder<W> {
    return SceneBuilder<W>(stack).Scene(std::move(name));
  }
private:
  std::shared_ptr<BuilderStack> stack;
};
/// @brief Starts a fluent description of a game: its scenes, and their entities and components.
///
/// The ergonomic path, in place of `CreateScene` / `CreateEntity` / `GetEntityComponent` written
/// out by hand. One expression describes a whole scene, and the builders track their own nesting
/// so no call has to be paired with a closing one:
///
///     Game(app, "Chess").Scene("Main").Entity<CameraObject>("Camera").ActiveCamera().Entity<MeshObject>("Pawn").Mesh("Cube");
///
/// The name is the window's title, which is the only effect it can honestly have.
///
/// Archetypes are checked against the game's world only when there is one: `World::Game` is this
/// with the world named.
template <typename W = AnyWorld>
auto Game(Application &app, const std::string &name = "") -> GameBuilder<W> {
  return GameBuilder<W>(app, name);
}
} // namespace kuki
