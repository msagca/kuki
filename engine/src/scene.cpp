#include <camera.hpp>
#include <entity_manager.hpp>
#include <id.hpp>
#include <scene.hpp>
#include <string>
#include <transform.hpp>
namespace kuki {
Scene::Scene(const std::string& name, unsigned int id)
  : name(name), id(id) {}
std::string Scene::GetName() const {
  return name;
}
unsigned int Scene::GetId() const {
  return id;
}
Camera* Scene::GetCamera() {
  // FIXME: the first camera may not be the active camera
  return entityManager.GetFirstComponent<Camera>();
}
ID Scene::CreateEntity(std::string& name) {
  return entityManager.Create(name);
}
void Scene::DeleteEntity(ID id) {
  entityManager.Delete(id);
  octree.Delete(id);
}
void Scene::DeleteEntity(const std::string& name) {
  auto id = entityManager.GetId(name);
  if (!id.IsValid())
    return;
  entityManager.Delete(id);
  octree.Delete(id);
}
void Scene::DeleteAllEntities() {
  entityManager.DeleteAll();
  octree.Clear();
}
void Scene::DeleteAllEntities(const std::string& prefix) {
  entityManager.DeleteAll(prefix);
  octree.Clear();
}
void Scene::SortTransforms() {
  entityManager.SortComponents<Transform>();
}
void Scene::UpdateTransforms() {
  entityManager.UpdateComponents<Transform>();
}
} // namespace kuki
