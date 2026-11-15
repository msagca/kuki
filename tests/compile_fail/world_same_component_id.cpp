// expect: two components have the same id
// Two of a game's components given the same index, which would share a column.
#include <world.hpp>
struct Health {
  int points{};
};
struct Armour {
  int points{};
};
KUKI_COMPONENT(Health, 0);
KUKI_COMPONENT(Armour, 0);
using Soldier = kuki::Archetype<kuki::Transform, Health, Armour>;
static_assert(kuki::World<Soldier>::Has<Soldier>);
