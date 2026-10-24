// expect: every parameter must be a component
// A query over something that is not a component.
#include <entity_manager.hpp>
auto Visit(kuki::EntityManager &manager) -> void {
  manager.ForEach<int>([](kuki::EntityID, int *) {});
}
