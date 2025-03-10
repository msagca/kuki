#include <component/camera.hpp>
#include <entity_manager.hpp>
#include <scene.hpp>
Scene::Scene(const std::string& name, unsigned int id)
  : name(name), id(id), entityManager() {}
std::string Scene::GetName() const {
  return name;
}
unsigned int Scene::GetID() const {
  return id;
}
Camera* Scene::GetCamera() {
  // FIXME: first camera may not be the active camera
  return entityManager.GetFirstComponent<Camera>();
}
EntityManager& Scene::GetEntityManager() {
  return entityManager;
}
