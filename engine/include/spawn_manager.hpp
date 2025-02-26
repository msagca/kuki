#pragma once
#include <engine_export.h>
#include <entity_manager.hpp>
class ENGINE_API SpawnManager {
private:
  EntityManager& entityManager;
  EntityManager& assetManager;
public:
  SpawnManager(EntityManager&, EntityManager&);
  int Spawn(std::string&, int = -1);
};
