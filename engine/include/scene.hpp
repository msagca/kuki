#pragma once
#include <asset_loader.hpp>
#include <engine_export.h>
#include <entity_manager.hpp>
#include <spawn_manager.hpp>
class ENGINE_API Scene {
private:
  EntityManager& assetManager;
  EntityManager entityManager;
  SpawnManager spawnManager;
public:
  Scene(EntityManager&);
  Camera* GetCamera();
  EntityManager& GetEntityManager();
  SpawnManager& GetSpawnManager();
};
