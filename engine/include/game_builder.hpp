#pragma once
#include <application.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <memory>
#include <script_registry.hpp>
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
/// One of these is created per `Application::Game` and held by every builder in the chain through
/// a `shared_ptr`, which is what lets each call return a builder by value without copying state.
///
/// The stack is the whole trick. A call names the level it needs rather than the level it is at,
/// and `PopTo` unwinds until it gets there -- so `.Script<T>().Entity("Pawn")` works without a
/// matching `End()`, because `Entity` asks for scene scope and the entity frame above it is
/// discarded on the way. Descending is explicit (`Child`) and so is coming back up (`Parent`),
/// because those are the two cases the level of a call cannot disambiguate: at entity scope a
/// second `Child` is a grandchild, never a sibling. That is the limitation of doing without
/// paired delimiters, and it is why anything with real hierarchy is better off calling
/// `CreateEntity` and `AddEntityComponent` directly.
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
class SceneBuilder;
/// @brief Fluent builder for one entity, and the level every component call lands on.
///
/// Returned by value throughout. It holds a `shared_ptr` and nothing else, so a chain of these
/// costs one allocation for the whole `Game(...)` expression rather than one per link.
///
/// Naming pitfall worth knowing: `Mesh`, `Script` and `Scene` are member names here and shadow
/// the engine types of those names inside this class. `With<T>` therefore wants a qualified type
/// for those three -- `With<kuki::Mesh>` rather than `With<Mesh>`.
class KUKI_ENGINE_API EntityBuilder {
public:
  explicit EntityBuilder(std::shared_ptr<BuilderStack> stack)
    : stack(std::move(stack)) {}
  /// @brief The entity being built, for the occasions a script has to be handed one.
  auto Id() const -> EntityID {
    return stack->Current();
  }
  /// @brief Points the entity at a mesh asset by the name the asset manager knows it by.
  ///
  /// A name that names nothing is logged and otherwise ignored, rather than asserting: a scene
  /// described in one expression is worth getting a partial picture of, and a missing mesh shows
  /// up as a missing object rather than as a dead process.
  auto Mesh(const std::string &) -> EntityBuilder;
  auto Mesh(const AssetID) -> EntityBuilder;
  auto Material(const std::string &) -> EntityBuilder;
  auto Material(const AssetID) -> EntityBuilder;
  auto Skybox(const std::string &) -> EntityBuilder;
  /// @brief Sets the local position, marking the transform so the change is actually applied.
  ///
  /// `Transform` carries no dirty bit, so every mutator here calls `MarkTransformDirty`. Doing
  /// that for you is most of the reason to reach for this over writing the component by hand.
  auto At(const glm::vec3 &) -> EntityBuilder;
  auto Scale(const glm::vec3 &) -> EntityBuilder;
  auto Scale(const float) -> EntityBuilder;
  auto Rotate(const glm::quat &) -> EntityBuilder;
  /// @brief Sets the rotation from yaw, pitch and roll in degrees.
  auto RotateEuler(const glm::vec3 &) -> EntityBuilder;
  /// @brief Makes this entity's camera the one the renderer draws from.
  auto ActiveCamera() -> EntityBuilder;
  /// @brief Adds a component and leaves it at its defaults.
  template <typename T>
  auto Add() -> EntityBuilder;
  /// @brief Adds a component and hands it to a callable to fill in.
  ///
  /// The escape hatch that keeps this class from needing a method per component type. Anything
  /// with no named convenience above is reached with `With<Light>([](Light &l) { ... })`.
  template <typename T>
  auto With(auto &&) -> EntityBuilder;
  /// @brief Attaches a script, registering the type so a saved scene can bring it back.
  ///
  /// The registration is the part worth pointing out. `ScriptRegistry::Register` is idempotent,
  /// and calling it here means a script placed through the builder round-trips through
  /// `SceneSerializer` without anyone having to remember `KUKI_REGISTER_SCRIPT` as well.
  template <typename T>
  auto Script() -> EntityBuilder;
  /// @brief Creates a child of the current entity and descends into it.
  auto Child(std::string = "") -> EntityBuilder;
  /// @brief Comes back up one level, so a sibling child can follow.
  ///
  /// At the top of a scene this leaves scope where it was; the builder returned is then only
  /// useful for `Entity` or `Scene`, both of which pop on their own account anyway.
  auto Parent() -> EntityBuilder;
  /// @brief Starts a new root entity in the current scene, abandoning whatever entity was open.
  auto Entity(std::string = "") -> EntityBuilder;
  /// @brief Starts a new scene, abandoning whatever entity and scene were open.
  auto Scene(std::string) -> SceneBuilder;
private:
  /// @brief The entity's transform, added if it has none. Null outside entity scope.
  auto GetTransform() -> Transform *;
  std::shared_ptr<BuilderStack> stack;
};
/// @brief Fluent builder for one scene: somewhere for entities to be created.
class KUKI_ENGINE_API SceneBuilder {
public:
  explicit SceneBuilder(std::shared_ptr<BuilderStack> stack)
    : stack(std::move(stack)) {}
  auto Entity(std::string = "") -> EntityBuilder;
  auto Scene(std::string) -> SceneBuilder;
private:
  std::shared_ptr<BuilderStack> stack;
};
/// @brief Root of the fluent builders. Obtained from `Application::Game`.
class KUKI_ENGINE_API GameBuilder {
public:
  GameBuilder(Application &, const std::string &);
  auto Scene(std::string) -> SceneBuilder;
private:
  std::shared_ptr<BuilderStack> stack;
};
template <typename T>
auto EntityBuilder::Add() -> EntityBuilder {
  if (const auto id = stack->Current(); id)
    stack->app.AddEntityComponent<T>(id);
  return *this;
}
template <typename T>
auto EntityBuilder::With(auto &&configure) -> EntityBuilder {
  const auto id = stack->Current();
  if (!id)
    return *this;
  if (auto *component = stack->app.AddEntityComponent<T>(id); component)
    configure(*component);
  return *this;
}
template <typename T>
auto EntityBuilder::Script() -> EntityBuilder {
  ScriptRegistry::Register<T>();
  if (const auto id = stack->Current(); id)
    stack->app.AddEntityComponent<T>(id);
  return *this;
}
} // namespace kuki
