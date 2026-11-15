#pragma once
#include <engine_archetypes.hpp>
#include <world.hpp>
namespace chess {
/// @brief The sky and the air the board is up in: the atmosphere, its clouds and fog, and the light
/// it throws back. One entity, so that one place says what the weather is.
using Settings = kuki::Archetype<kuki::Transform, kuki::AtmosphereSky, kuki::VolumetricClouds, kuki::VolumetricFog, kuki::IndirectLighting>;
/// @brief Everything a scene of the chess game can be made of. Every piece, square, glyph and part of
/// the clock is a mesh, and the clock's parts hang from an empty node, so the engine's archetypes do
/// for all of it bar the settings.
using ChessWorld = kuki::World<Settings>;
} // namespace chess
