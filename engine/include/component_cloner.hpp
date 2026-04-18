#pragma once
#include <entity_manager.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
namespace kuki {
class KUKI_ENGINE_API ComponentCloner {
public:
  ComponentCloner(EntityManager &, const EntityID = EntityID::Invalid);
  EntityID entityId{};
  template <typename T>
  auto operator()(const T *) -> void;
private:
  EntityManager &entityManager;
};
template <typename T>
auto ComponentCloner::operator()(const T *other) -> void {
  if (!entityId || !other)
    return;
  if constexpr (std::is_base_of_v<Script, T>)
    other->CloneTo(entityManager, entityId);
  else {
    auto component = entityManager.AddComponent<T>(entityId);
    *component = *other;
  }
}
} // namespace kuki
