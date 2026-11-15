#pragma once
#include <component_traits.hpp>
#include <engine_archetypes.hpp>
#include <world.hpp>
namespace my_game {
/// @brief How fast an entity turns about its Y axis, in radians a second.
///
/// A game's component is plain data, as every component is: no methods that change it, nothing it
/// owns. What makes it turn is `SpinSystem`.
struct Spin {
  float speed{1.f};
};
} // namespace my_game
// At global scope, after the type is complete. Indices are the game's own, counted from nought.
KUKI_COMPONENT(my_game::Spin, 0);
namespace my_game {
/// @brief Something drawn that turns.
using Spinner = kuki::Archetype<kuki::Transform, kuki::Renderable, Spin>;
/// @brief The sky, and the light it throws back on the scene.
using Sky = kuki::Archetype<kuki::Transform, kuki::AtmosphereSky, kuki::IndirectLighting>;
/// @brief Every archetype the game's scenes hold, besides the engine's own (`MeshObject`,
/// `CameraObject`, `LightObject`, ...), which come with every world.
using MyGameWorld = kuki::World<Spinner, Sky>;
} // namespace my_game
