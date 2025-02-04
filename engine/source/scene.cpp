#include <asset_manager.hpp>
#include <entity_manager.hpp>
#include <scene.hpp>
#include <spawn_manager.hpp>
Scene::Scene(AssetManager& assetManager)
  : assetManager(assetManager), entityManager(), spawnManager(entityManager, assetManager) {}
EntityManager& Scene::GetEntityManager() {
  return entityManager;
}
SpawnManager& Scene::GetSpawnManager() {
  return spawnManager;
}
