// expect: two archetypes have the same components
// A game's archetype that is one of the engine's written in another order.
#include <light.hpp>
#include <world.hpp>
using Lamp = kuki::Archetype<kuki::Light, kuki::Transform>;
static_assert(kuki::World<Lamp>::Has<Lamp>);
