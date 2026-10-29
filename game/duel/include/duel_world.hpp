#pragma once
#include <component_traits.hpp>
#include <engine_archetypes.hpp>
#include <world.hpp>
namespace duel {
/// @brief Which player a figure in the scene is the picture of.
///
/// The rules know the players by number and the scene knows the figures by entity, and this is
/// what joins the two: `DuelSystem` finds the figures by asking for the entities with a seat,
/// rather than keeping a list of them beside the scene that the scene could fall out of step with.
struct Seat {
  int player{};
};
} // namespace duel
KUKI_COMPONENT(duel::Seat, 0);
namespace duel {
/// @brief A figure: a placed model that can play its clips, and the seat it is drawn for.
using Figure = kuki::Archetype<kuki::Transform, kuki::BoundingBox, kuki::Animated, Seat>;
/// @brief The sky, which is both the atmosphere and the light it throws back on the scene.
using Sky = kuki::Archetype<kuki::Transform, kuki::AtmosphereSky, kuki::IndirectLighting>;
/// @brief How the picture is drawn -- cel bands, ink, a grade -- and what the fight is doing to it
/// this instant: the flash and the speed lines of a blow landing.
using Look = kuki::Archetype<kuki::Transform, kuki::Stylization, kuki::ScreenEffects>;
/// @brief Everything a scene of the duel can be made of: the engine's archetypes, and these.
using DuelWorld = kuki::World<Figure, Sky, Look>;
} // namespace duel
