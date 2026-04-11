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
  auto AddEntityComponent(const EntityID, const ComponentType) -> void;
  auto CopyEntityFrom(const EntityManager &, const EntityID) -> EntityID;
  auto CopyEntityTo(const EntityID, EntityManager &) const -> EntityID;
  auto CreateEntity(std::string) -> EntityID;
  auto DeleteEntities() -> void;
  auto DeleteEntity(const EntityID) -> bool;
  auto EntityHasChildren(const EntityID) const -> bool;
  auto EntityHasParent(const EntityID) const -> bool;
  auto GetEntityComponent(const EntityID, const ComponentType) -> std::optional<ComponentVariant>;
  auto GetEntityComponentTypes(const EntityID) const -> std::vector<ComponentType>;
  auto GetEntityCount() const -> size_t;
  auto GetEntityName(const EntityID) const -> std::string;
  auto GetMissingEntityComponents(const EntityID) const -> std::vector<ComponentType>;
  auto IsEntity(const EntityID) const -> bool;
  auto RemoveEntityComponent(const EntityID, const ComponentType) -> bool;
  auto RenameEntity(const EntityID, std::string) -> bool;
  // templates
  auto ForEachChildEntity(this auto &, const EntityID, auto &&) -> void;
  auto ForEachRootEntity(this auto &, auto &&) -> void;
  auto GetActiveCamera(this auto &) -> decltype(auto);
  template <typename... T>
  auto AddEntityComponent(const EntityID) -> decltype(auto);
  template <typename... T>
  auto EntityHasComponent(const EntityID) const -> bool;
  template <typename... T>
  auto ForEachEntity(this auto &, auto &&) -> void;
  template <typename... T>
  auto ForFirstEntity(this auto &, auto &&) -> void;
  template <typename T>
  auto GetAnyComponent(this auto &) -> decltype(auto);
  template <typename... T>
  auto GetEntityComponent(this auto &, const EntityID) -> decltype(auto);
  template <typename... T>
  auto SortComponents() -> void;
  template <typename... T>
  auto UpdateComponents() -> void;
  template <typename... T>
  auto RemoveEntityComponent(const EntityID) -> bool;
private:
  EntityManager entityManager{};
};
auto Scene::ForEachChildEntity(this auto &self, const EntityID id, auto &&func) -> void {
  self.entityManager.ForEachChild(id, std::forward<decltype(func)>(func));
}
auto Scene::ForEachRootEntity(this auto &self, auto &&func) -> void {
  self.entityManager.ForEachRoot(std::forward<decltype(func)>(func));
}
auto Scene::GetActiveCamera(this auto &self) -> decltype(auto) {
  return self.entityManager.template GetAny<Camera>();
}
template <typename... T>
auto Scene::AddEntityComponent(const EntityID id) -> decltype(auto) {
  return entityManager.AddComponent<T...>(id);
}
template <typename... T>
auto Scene::EntityHasComponent(const EntityID id) const -> bool {
  return entityManager.HasComponent<T...>(id);
}
template <typename... T>
auto Scene::ForEachEntity(this auto &self, auto &&func) -> void {
  self.entityManager.template ForEach<T...>(std::forward<decltype(func)>(func));
}
template <typename... T>
auto Scene::ForFirstEntity(this auto &self, auto &&func) -> void {
  self.entityManager.template ForFirst<T...>(std::forward<decltype(func)>(func));
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
auto Scene::SortComponents() -> void {
  entityManager.SortComponents<T...>();
}
template <typename... T>
auto Scene::UpdateComponents() -> void {
  entityManager.UpdateComponents<T...>();
}
template <typename... T>
auto Scene::RemoveEntityComponent(const EntityID id) -> bool {
  return entityManager.RemoveComponent<T...>(id);
}
} // namespace kuki
