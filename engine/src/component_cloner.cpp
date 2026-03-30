#include <component_cloner.hpp>
namespace kuki {
ComponentCloner::ComponentCloner(EntityManager &entityManager, const EntityID entityId)
  : entityManager(entityManager), entityId(entityId) {}
} // namespace kuki
