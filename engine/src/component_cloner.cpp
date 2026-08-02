#include <component_cloner.hpp>
#include <entity_manager.hpp>
#include <id.hpp>
namespace kuki {
ComponentCloner::ComponentCloner(EntityManager &entityManager, const EntityID entityId)
  : entityManager(entityManager), entityId(entityId) {}
} // namespace kuki
