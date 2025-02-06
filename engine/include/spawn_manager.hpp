#pragma once
#include <engine_export.h>
#include <asset_manager.hpp>
#include <entity_manager.hpp>
class ENGINE_API SpawnManager {
private:
  EntityManager& entityManager;
  AssetManager& assetManager;
public:
  SpawnManager(EntityManager&, AssetManager&);
  int Spawn(const std::string&, int = -1);
};
