#pragma once
#include <cstdint>
namespace kuki {
/// @brief The engine's own components, one enumerator each, whose value is the component's id.
///
/// Only what an entity can carry. The backends' per-entity state is one `RenderBinding` rather
/// than a component per backend, and the GPU resources a backend keeps for itself (`GLBuffer`,
/// `GLTexture` and the like) are its own business and live in its own pools.
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
  MeshHandle,
  ModelMaterialHandle,
  ModelMeshHandle,
  ParticleEmitter,
  RenderBinding,
  Skeleton,
  SkyboxHandle,
  TextureHandle,
  Transform,
  VolumetricClouds,
  VolumetricFog
};
/// @brief How many engine components there are, from the last enumerator.
inline constexpr auto ENGINE_COMPONENT_COUNT = static_cast<uint8_t>(ComponentType::VolumetricFog) + 1;
} // namespace kuki
