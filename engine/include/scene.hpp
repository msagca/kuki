#pragma once
#include <kuki_export.h>
#include <entity_manager.hpp>
#include <utility/octree.hpp>
class KUKI_API Scene {
private:
  std::string name;
  unsigned int id;
  EntityManager entityManager;
public:
  Scene(const std::string&, unsigned int);
  std::string GetName() const;
  unsigned int GetId() const;
  Camera* GetCamera();
  // TODO: add methods to remove the need to refer to the scene's entity manager
  EntityManager& GetEntityManager();
};
