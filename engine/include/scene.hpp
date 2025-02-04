#pragma once
#include <engine_export.h>
#include <asset_loader.hpp>
#include <entity_manager.hpp>
#include <spawn_manager.hpp>
class ENGINE_EXPORT Scene {
private:
  AssetManager& assetManager;
  EntityManager entityManager;
  SpawnManager spawnManager;
public:
  Scene(AssetManager&);
  EntityManager& GetEntityManager();
  SpawnManager& GetSpawnManager();
};
