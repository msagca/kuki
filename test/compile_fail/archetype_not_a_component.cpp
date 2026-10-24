// expect: an archetype names a type that is not a component
// A type nobody declared to be a component, which a type_index lookup used to answer as a transform.
#include <engine_archetypes.hpp>
struct Speed {
  float value{};
};
using Runner = kuki::Archetype<kuki::Transform, Speed>;
static_assert(Runner::bits != 0);
