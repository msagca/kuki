#include <application.hpp>
#include <game_builder.hpp>
#include <mesh_asset.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>
namespace kuki::builder {
auto ResolveAsset(Application &app, const std::string &name) -> AssetID {
  if (const auto *asset = app.GetAsset(name); asset)
    return asset->id;
  spdlog::warn("[GameBuilder] No asset named '{}'", name);
  return {};
}
auto BeginGame(Application &app, const std::string &name) -> void {
  // The name is the window's, which is the one place a game's title is visible without a text
  // renderer -- and the only effect this argument can honestly have.
  if (!name.empty())
    app.SetWindowTitle(name);
}
auto BeginScene(BuilderStack &stack, std::string name) -> void {
  stack.PopTo(BuilderScope::Game);
  stack.app.CreateScene(name);
  // Created and then switched to, because `SceneManager::Create` only adopts a scene as the
  // active one when there is not already one -- so the second `Scene(...)` in a chain would
  // otherwise create a scene and go on filling the first.
  stack.app.SwitchScene(name);
  stack.Push(BuilderScope::Scene);
}
auto BeginEntity(BuilderStack &stack, const ComponentMask &mask, std::string name) -> void {
  stack.PopTo(BuilderScope::Scene);
  // An archetype the scene was never told about makes no entity, and says so; the frame is pushed
  // anyway with the invalid id, and every call checks the frame before touching anything.
  stack.Push(BuilderScope::Entity, stack.app.CreateEntity(mask, std::move(name)));
}
auto ReopenEntity(BuilderStack &stack, const ComponentMask &mask, const EntityID id) -> void {
  stack.PopTo(BuilderScope::Scene);
  auto valid = id && stack.app.IsEntity(id);
  if (valid && (stack.app.GetEntitySignature(id) & mask) != mask) {
    spdlog::error("[GameBuilder] {} is not of the archetype it was reopened as, so the calls after it edit nothing", stack.app.GetEntityName(id));
    valid = false;
  }
  stack.Push(BuilderScope::Entity, valid ? id : EntityID::Invalid);
}
auto BeginChild(BuilderStack &stack, const ComponentMask &mask, std::string name) -> void {
  const auto parent = stack.Current();
  const auto id = stack.app.CreateEntity(mask, std::move(name));
  if (id && parent)
    stack.app.AddChildEntity(parent, id);
  stack.Push(BuilderScope::Entity, id);
}
auto MarkMoved(BuilderStack &stack) -> void {
  stack.app.MarkTransformDirty(stack.Current());
}
auto SetMesh(BuilderStack &stack, const AssetID mesh) -> void {
  const auto id = stack.Current();
  if (!id)
    return;
  auto pool = stack.app.GetEntityPool(id);
  pool.mesh = mesh;
  pool.meshIndex = PoolKey::NONE;
  if (!pool.material)
    if (const auto *asset = stack.app.GetAsset<MeshAsset>(mesh))
      pool.material = asset->material;
  stack.app.MoveEntityToPool(id, pool);
}
auto SetMaterial(BuilderStack &stack, const AssetID material) -> void {
  const auto id = stack.Current();
  if (!id)
    return;
  auto pool = stack.app.GetEntityPool(id);
  if (!pool) {
    spdlog::warn("[GameBuilder] '{}' was given a material before a mesh; a material is a pool's, and an entity with no mesh is in no pool", stack.app.GetEntityName(id));
    return;
  }
  pool.material = material;
  pool.materialIndex = PoolKey::NONE;
  stack.app.MoveEntityToPool(id, pool);
}
} // namespace kuki::builder
