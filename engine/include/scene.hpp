#pragma once
#include <entity_manager.hpp>
#include <kuki_export.h>
#include <utility/octree.hpp>
namespace kuki {
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
} // namespace kuki
