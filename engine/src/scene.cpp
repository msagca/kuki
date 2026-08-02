#include <asset_manager.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <component_type.hpp>
#include <entity_manager.hpp>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <optional>
#include <scene.hpp>
#include <string>
#include <transform.hpp>
#include <utility>
#include <vector>
namespace kuki {
Scene::Scene(const SceneID id)
  : id(id) {}
auto Scene::AlignView(const EntityID id) -> void {
  constexpr auto ORIENTATION = glm::vec3{0.f};
  constexpr auto DISTANCE_FACTOR = 2.f;
  auto camera = GetCamera();
  if (!camera)
    return;
  const auto [bounds, transform] = entityManager.GetComponent<BoundingBox, Transform>(id);
  if (!bounds || !transform)
    return;
  camera->Frame(bounds->GetWorldBounds(transform->world), transform->position, ORIENTATION, DISTANCE_FACTOR);
}
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
  return entityManager.Create(std::move(name));
}
auto Scene::DeleteEntities() -> void {
  entityManager.Clear();
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
auto Scene::GetEntityComponent(const EntityID id, const ComponentType type) -> std::optional<ComponentVariant> {
  return entityManager.GetComponent(id, type);
}
auto Scene::GetEntityComponentTypes(const EntityID id) const -> std::vector<ComponentType> {
  return entityManager.GetComponentTypes(id);
}
auto Scene::GetEntityCount() const -> size_t {
  return entityManager.GetCount();
}
auto Scene::GetEntityName(const EntityID id) const -> std::string {
  return entityManager.GetName(id);
}
auto Scene::GetParent(const EntityID id) const -> EntityID {
  return entityManager.GetParent(id);
}
auto Scene::GetMissingEntityComponents(const EntityID id) const -> std::vector<ComponentType> {
  return entityManager.GetMissingComponentTypes(id);
}
auto Scene::GetStructuralGeneration() const -> size_t {
  return entityManager.GetStructuralGeneration();
}
auto Scene::IsEntity(const EntityID id) const -> bool {
  return entityManager.IsEntity(id);
}
auto Scene::RemoveEntityScript(const EntityID id, const std::type_index type) -> bool {
  return entityManager.RemoveScript(id, type);
}
auto Scene::RenameEntity(const EntityID id, std::string name) -> bool {
  return entityManager.Rename(id, std::move(name));
}
auto Scene::SetActiveCamera(const EntityID id) -> bool {
  if (!entityManager.HasComponent<Camera>(id))
    return false;
  activeCameraId = id;
  return true;
}
} // namespace kuki
