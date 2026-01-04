#include <asset_manager.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <entity_manager.hpp>
#include <id.hpp>
#include <scene.hpp>
#include <string>
#include <transform.hpp>
namespace kuki {
Scene::Scene(const SceneID id)
  : id(id) {}
auto Scene::AddChildEntity(const EntityID parent, const EntityID child) -> bool {
  return entityManager.AddChild(parent, child);
}
auto Scene::CopyEntityFrom(const EntityManager &otherManager, const EntityID id) -> EntityID {
  return entityManager.CopyFrom(otherManager, id);
}
auto Scene::CopyEntityTo(const EntityID id, EntityManager &otherManager) const -> EntityID {
  return entityManager.CopyTo(id, otherManager);
}
auto Scene::CreateEntity(std::string name) -> EntityID {
  return entityManager.Create(name);
}
auto Scene::DeleteEntities() -> void {
  entityManager.DeleteAll();
}
auto Scene::DeleteEntity(const EntityID id) -> bool {
  return entityManager.Delete(id);
}
auto Scene::EntityHasChildren(const EntityID id) const -> bool {
  return entityManager.HasChildren(id);
}
auto Scene::EntityHasParent(const EntityID id) const -> bool {
  return entityManager.HasParent(id);
}
auto Scene::GetEntityComponents(const EntityID id) const -> std::vector<ComponentType> {
  return entityManager.GetComponents(id);
}
auto Scene::GetEntityCount() const -> size_t {
  return entityManager.GetCount();
}
auto Scene::GetEntityName(const EntityID id) const -> std::string {
  return entityManager.GetName(id);
}
auto Scene::GetID() const -> SceneID {
  return id;
}
auto Scene::GetMissingEntityComponents(const EntityID id) const -> std::vector<ComponentType> {
  return entityManager.GetMissingComponents(id);
}
auto Scene::IsEntity(const EntityID id) const -> bool {
  return entityManager.IsEntity(id);
}
auto Scene::RenameEntity(const EntityID id, const std::string &name) -> bool {
  return entityManager.Rename(id, name);
}
} // namespace kuki
