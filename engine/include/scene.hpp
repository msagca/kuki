#pragma once
#include <camera.hpp>
#include <component.hpp>
#include <component_type.hpp>
#include <concepts.hpp>
#include <entity_manager.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <string>
namespace kuki {
class KUKI_ENGINE_API Scene {
public:
  Scene(const SceneID);
  const SceneID id;
  auto AddChildEntity(const EntityID, const EntityID) -> bool;
  auto AlignView(const EntityID) -> void;
  auto CopyEntityFrom(const EntityManager &, const EntityID) -> EntityID;
  auto CopyEntityTo(const EntityID, EntityManager &) const -> EntityID;
  /// @brief Tells the scene about an archetype. Done for every archetype of the application's
  /// `World` when the scene is made, so seldom by hand.
  auto Declare(const ArchetypeSpec &) -> void;
  auto IsDeclared(const ComponentMask &) const -> bool;
  /// @brief Makes an entity of the archetype with these components. See `EntityManager::Create`.
  auto CreateEntity(const ComponentMask &, std::string) -> EntityID;
  template <IsArchetype A>
  auto CreateEntity(std::string) -> EntityID;
  /// @brief Copies the components two entities share, from one of another manager to one here.
  auto CopyEntityComponents(const EntityManager &, const EntityID from, const EntityID to) -> ComponentMask;
  auto DeleteEntities() -> void;
  auto DeleteEntity(const EntityID) -> bool;
  auto EntityHasChildren(const EntityID) const -> bool;
  auto ForEachChildEntity(this auto &, const EntityID, auto &&) -> void;
  auto ForEachRootEntity(this auto &, auto &&) -> void;
  auto GetActiveCamera(this auto &) -> decltype(auto);
  auto GetCamera(this auto &, const std::string & = "") -> decltype(auto);
  auto GetEntityComponent(const EntityID, const ComponentType) -> std::optional<ComponentVariant>;
  auto GetEntityComponentTypes(const EntityID) const -> std::vector<ComponentType>;
  auto GetEntityCount() const -> size_t;
  auto GetEntityName(const EntityID) const -> std::string;
  auto GetParent(const EntityID) const -> EntityID;
  auto GetEntitySignature(const EntityID) const -> ComponentMask;
  auto GetStructuralGeneration() const -> size_t;
  auto HasComponentAnywhere(const ComponentType) const -> bool;
  auto IsEntity(const EntityID) const -> bool;
  /// @brief Flags an entity's transform, and everything below it, for recomputation.
  ///
  /// Call after writing to a transform's position, rotation or scale; the component carries no
  /// dirty bit of its own, so an unannounced change goes unapplied.
  auto MarkTransformDirty(const EntityID) -> void;
  auto RenameEntity(const EntityID, std::string) -> bool;
  auto SetActiveCamera(const EntityID) -> bool;
  template <typename... T>
  auto EntityHasComponent(const EntityID) const -> bool;
  template <typename... T>
  auto ForEachEntity(this auto &, auto &&) -> void;
  template <IsArchetype A, typename... T>
  auto ForEachEntityOf(this auto &, auto &&) -> void;
  template <typename T>
  auto GetAnyComponent(this auto &) -> decltype(auto);
  template <typename... T>
  auto GetEntityComponent(this auto &, const EntityID) -> decltype(auto);
  template <typename... T>
  auto UpdateComponents() -> void;
private:
  EntityManager entityManager{};
  EntityID activeCameraId{};
};
auto Scene::ForEachChildEntity(this auto &self, const EntityID id, auto &&func) -> void {
  self.entityManager.ForEachChild(id, std::forward<decltype(func)>(func));
}
auto Scene::ForEachRootEntity(this auto &self, auto &&func) -> void {
  self.entityManager.ForEachRoot(std::forward<decltype(func)>(func));
}
auto Scene::GetActiveCamera(this auto &self) -> decltype(auto) {
  if (self.activeCameraId)
    if (auto *camera = self.entityManager.template GetComponent<Camera>(self.activeCameraId); camera)
      return camera;
  return self.entityManager.template GetAny<Camera>();
}
auto Scene::GetCamera(this auto &self, const std::string &name) -> decltype(auto) {
  if (name.empty())
    return self.GetActiveCamera();
  return self.entityManager.template GetComponent<Camera>(name);
}
template <IsArchetype A>
auto Scene::CreateEntity(std::string name) -> EntityID {
  return entityManager.Create<A>(std::move(name));
}
template <typename... T>
auto Scene::EntityHasComponent(const EntityID id) const -> bool {
  return entityManager.HasComponent<T...>(id);
}
template <typename... T>
auto Scene::ForEachEntity(this auto &self, auto &&func) -> void {
  self.entityManager.template ForEach<T...>(std::forward<decltype(func)>(func));
}
template <IsArchetype A, typename... T>
auto Scene::ForEachEntityOf(this auto &self, auto &&func) -> void {
  self.entityManager.template ForEachOf<A, T...>(std::forward<decltype(func)>(func));
}
template <typename T>
auto Scene::GetAnyComponent(this auto &self) -> decltype(auto) {
  return self.entityManager.template GetAny<T>();
}
template <typename... T>
auto Scene::GetEntityComponent(this auto &self, const EntityID id) -> decltype(auto) {
  return self.entityManager.template GetComponent<T...>(id);
}
template <typename... T>
auto Scene::UpdateComponents() -> void {
  entityManager.UpdateComponents<T...>();
}
} // namespace kuki
