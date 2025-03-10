#pragma once
#include <engine_export.h>
#include <entity_manager.hpp>
class ENGINE_API Scene {
private:
  std::string name;
  unsigned int id;
  EntityManager entityManager;
public:
  Scene(const std::string&, unsigned int);
  std::string GetName() const;
  unsigned int GetID() const;
  Camera* GetCamera();
  EntityManager& GetEntityManager();
};
