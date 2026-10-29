#pragma once
#include <component_traits.hpp>
#include <engine_archetypes.hpp>
#include <world.hpp>
namespace frontier {
/// @brief Which building a piece of the scene belongs to, by its place in the placement system's
/// list. The rules keep buildings by id and the scene keeps their meshes, and this is the join.
struct Structure {
  int id{-1};
};
/// @brief One part of a bird: which flock and which bird it is, and whether it is the body or a wing.
struct Wing {
  int flock{};
  int bird{};
  /// Nought for the body, minus one for the left wing and one for the right.
  int side{};
};
} // namespace frontier
KUKI_COMPONENT(frontier::Structure, 0);
KUKI_COMPONENT(frontier::Wing, 1);
namespace frontier {
/// @brief A building's mesh: drawn, and found again by its building when the building is removed.
using StructureObject = kuki::Archetype<kuki::Transform, kuki::Renderable, Structure>;
/// @brief A bird's body or one of its wings, posed by the flock system every frame.
using BirdObject = kuki::Archetype<kuki::Transform, kuki::Renderable, Wing>;
/// @brief The sky, which is both the atmosphere and the light it throws back on the scene.
using Sky = kuki::Archetype<kuki::Transform, kuki::AtmosphereSky, kuki::IndirectLighting>;
/// @brief How the picture is drawn, and the water's mirror: the sea is the one level surface.
using Look = kuki::Archetype<kuki::Transform, kuki::Stylization, kuki::ScreenEffects, kuki::PlanarReflection>;
using FrontierWorld = kuki::World<StructureObject, BirdObject, Sky, Look>;
} // namespace frontier
