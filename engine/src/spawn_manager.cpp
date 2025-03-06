#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/mesh_filter.hpp>
#include <component/mesh_renderer.hpp>
#include <component/transform.hpp>
#include <entity_manager.hpp>
#include <spawn_manager.hpp>
#include <string>
SpawnManager::SpawnManager(EntityManager& entityManager, EntityManager& assetManager)
  : entityManager(entityManager), assetManager(assetManager) {}
int SpawnManager::Spawn(std::string& name, int parentID) {
  auto assetID = assetManager.GetID(name);
  if (assetID < 0)
    return -1;
  auto entityID = entityManager.Create(name);
  auto components = assetManager.GetAllComponents(assetID);
  for (auto c : components)
    if (auto t = dynamic_cast<Transform*>(c)) {
      auto transform = entityManager.AddComponent<Transform>(entityID);
      *transform = *t;
      transform->parent = parentID;
    } else if (auto m = dynamic_cast<Mesh*>(c)) {
      auto filter = entityManager.AddComponent<MeshFilter>(entityID);
      filter->mesh = *m;
    } else if (auto m = dynamic_cast<Material*>(c)) {
      auto renderer = entityManager.AddComponent<MeshRenderer>(entityID);
      renderer->material = *m;
    }
  assetManager.ForEachChild(assetID, [this, &entityID](unsigned int id) {
    auto name = assetManager.GetName(id);
    auto childID = Spawn(name, entityID);
    entityManager.AddChild(entityID, childID);
  });
  return entityID;
}
