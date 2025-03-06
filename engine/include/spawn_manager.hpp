#pragma once
#include <engine_export.h>
#include <entity_manager.hpp>
class ENGINE_API SpawnManager {
private:
  EntityManager& assetManager;
  EntityManager& entityManager;
public:
  SpawnManager(EntityManager&, EntityManager&);
  /// <summary>
  /// Turn an asset into an entity (copy data from asset manager to entity manager)
  /// </summary>
  int Spawn(std::string&, int = -1);
};
