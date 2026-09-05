#include <camera.hpp>
#include <game_builder.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <material_handle.hpp>
#include <mesh_handle.hpp>
#include <skybox_handle.hpp>
#include <spdlog/spdlog.h>
#include <transform.hpp>
#include <utility>
namespace kuki {
namespace {
  /// @brief Resolves an asset name, complaining once rather than leaving a silent empty handle.
  auto ResolveAsset(Application &app, const std::string &name) -> AssetID {
    if (const auto *asset = app.GetAsset(name); asset)
      return asset->id;
    spdlog::warn("[GameBuilder] no asset named '{}'", name);
    return {};
  }
} // namespace
GameBuilder::GameBuilder(Application &app, const std::string &name)
  : stack(std::make_shared<BuilderStack>(app)) {
  // The name is the window's, which is the one place a game's title is visible without a text
  // renderer -- and the only effect this argument can honestly have.
  if (!name.empty())
    app.SetWindowTitle(name);
}
auto GameBuilder::Scene(std::string name) -> SceneBuilder {
  SceneBuilder builder(stack);
  return builder.Scene(std::move(name));
}
auto SceneBuilder::Scene(std::string name) -> SceneBuilder {
  stack->PopTo(BuilderScope::Game);
  stack->app.CreateScene(name);
  // Created and then switched to, because `SceneManager::Create` only adopts a scene as the
  // active one when there is not already one -- so the second `Scene(...)` in a chain would
  // otherwise create a scene and go on filling the first.
  stack->app.SwitchScene(name);
  stack->Push(BuilderScope::Scene);
  return SceneBuilder(stack);
}
auto SceneBuilder::Entity(std::string name) -> EntityBuilder {
  stack->PopTo(BuilderScope::Scene);
  const auto id = stack->app.CreateEntity(std::move(name));
  // Every entity gets a transform. A mesh without one is invisible, and an entity that is only a
  // script does not mind carrying one -- which makes always adding it the choice that surprises
  // nobody. Adding a component moves the entity between archetypes, so this needs no dirty mark.
  if (id)
    stack->app.AddEntityComponent<Transform>(id);
  stack->Push(BuilderScope::Entity, id);
  return EntityBuilder(stack);
}
auto EntityBuilder::Entity(std::string name) -> EntityBuilder {
  SceneBuilder builder(stack);
  return builder.Entity(std::move(name));
}
auto EntityBuilder::Scene(std::string name) -> SceneBuilder {
  SceneBuilder builder(stack);
  return builder.Scene(std::move(name));
}
auto EntityBuilder::Child(std::string name) -> EntityBuilder {
  const auto parent = stack->Current();
  const auto id = stack->app.CreateEntity(std::move(name));
  if (id) {
    stack->app.AddEntityComponent<Transform>(id);
    if (parent)
      stack->app.AddChildEntity(parent, id);
  }
  stack->Push(BuilderScope::Entity, id);
  return EntityBuilder(stack);
}
auto EntityBuilder::Parent() -> EntityBuilder {
  stack->Pop();
  return EntityBuilder(stack);
}
auto EntityBuilder::GetTransform() -> Transform * {
  const auto id = stack->Current();
  if (!id)
    return nullptr;
  return stack->app.AddEntityComponent<Transform>(id);
}
auto EntityBuilder::Mesh(const std::string &name) -> EntityBuilder {
  return Mesh(ResolveAsset(stack->app, name));
}
auto EntityBuilder::Mesh(const AssetID asset) -> EntityBuilder {
  const auto id = stack->Current();
  if (id && asset)
    if (auto *handle = stack->app.AddEntityComponent<MeshHandle>(id); handle)
      handle->assetId = asset;
  return *this;
}
auto EntityBuilder::Material(const std::string &name) -> EntityBuilder {
  return Material(ResolveAsset(stack->app, name));
}
auto EntityBuilder::Material(const AssetID asset) -> EntityBuilder {
  const auto id = stack->Current();
  if (id && asset)
    if (auto *handle = stack->app.AddEntityComponent<MaterialHandle>(id); handle)
      handle->assetId = asset;
  return *this;
}
auto EntityBuilder::Skybox(const std::string &name) -> EntityBuilder {
  const auto asset = ResolveAsset(stack->app, name);
  const auto id = stack->Current();
  if (id && asset)
    if (auto *handle = stack->app.AddEntityComponent<SkyboxHandle>(id); handle)
      handle->assetId = asset;
  return *this;
}
auto EntityBuilder::At(const glm::vec3 &position) -> EntityBuilder {
  if (auto *transform = GetTransform(); transform) {
    transform->position = position;
    stack->app.MarkTransformDirty(stack->Current());
  }
  return *this;
}
auto EntityBuilder::Scale(const glm::vec3 &scale) -> EntityBuilder {
  if (auto *transform = GetTransform(); transform) {
    transform->scale = scale;
    stack->app.MarkTransformDirty(stack->Current());
  }
  return *this;
}
auto EntityBuilder::Scale(const float scale) -> EntityBuilder {
  return Scale(glm::vec3(scale));
}
auto EntityBuilder::Rotate(const glm::quat &rotation) -> EntityBuilder {
  if (auto *transform = GetTransform(); transform) {
    transform->rotation = rotation;
    stack->app.MarkTransformDirty(stack->Current());
  }
  return *this;
}
auto EntityBuilder::RotateEuler(const glm::vec3 &degrees) -> EntityBuilder {
  return Rotate(glm::quat(glm::radians(degrees)));
}
auto EntityBuilder::ActiveCamera() -> EntityBuilder {
  const auto id = stack->Current();
  if (!id)
    return *this;
  stack->app.AddEntityComponent<Camera>(id);
  stack->app.SetActiveCamera(id);
  return *this;
}
} // namespace kuki
