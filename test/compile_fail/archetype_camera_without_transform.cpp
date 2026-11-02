// expect: has no Transform to place it by
// A camera declared without a transform, which would sit at the origin whatever the game did to it.
#include <camera.hpp>
#include <engine_archetypes.hpp>
using Lens = kuki::Archetype<kuki::Camera>;
static_assert(Lens::bits != 0);
