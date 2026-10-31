#pragma once
#include <cstdint>
namespace kuki {
/// @brief The engine's own components, one enumerator each, whose value is the component's id.
///
/// Only what an entity can carry. What a drawn entity is drawn with is its pool's (`Pooled`,
/// `pool.hpp`), not a component of its own, and the GPU resources a backend keeps for itself are
/// its own business.
///
/// A game's components are not here: they take ids from `FIRST_GAME_COMPONENT` up, through
/// `KUKI_COMPONENT` (`component_traits.hpp`), so a game never edits the engine to add one.
enum class ComponentType : uint8_t {
  Animator,
  AntiAliasing,
  AtmosphereSky,
  BoundingBox,
  Camera,
  IndirectLighting,
  Light,
  MaterialHandle,
  ParticleEmitter,
  PlanarReflection,
  Pooled,
  ScreenEffects,
  Skeleton,
  SkyboxHandle,
  Stylization,
  TextureHandle,
  Transform,
  VolumetricClouds,
  VolumetricFog,
  Wind,
  WorldMatrix
};
/// @brief How many engine components there are, from the last enumerator.
inline constexpr auto ENGINE_COMPONENT_COUNT = static_cast<uint8_t>(ComponentType::WorldMatrix) + 1;
} // namespace kuki
