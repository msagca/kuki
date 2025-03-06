#include <entity_manager.hpp>
#include <scene.hpp>
#include <spawn_manager.hpp>
#include <component/camera.hpp>
Scene::Scene(EntityManager& assetManager)
  : assetManager(assetManager), entityManager(), spawnManager(entityManager, assetManager) {}
Camera* Scene::GetCamera() {
  // FIXME: first camera may not be the active camera
  return entityManager.GetFirstComponent<Camera>();
}
EntityManager& Scene::GetEntityManager() {
  return entityManager;
}
SpawnManager& Scene::GetSpawnManager() {
  return spawnManager;
}
