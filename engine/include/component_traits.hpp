#pragma once
#include <bitset>
#include <component_type.hpp>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <macros.hpp>
#include <string_view>
#include <type_list.hpp>
namespace kuki {
/// @brief A component's bit in an archetype's signature.
using ComponentId = uint8_t;
/// @brief How many component types one application can have, the engine's and its game's together.
inline constexpr size_t MAX_COMPONENTS = 64;
/// @brief The first id a game's own components take. The engine keeps the ones below for itself,
/// with room to grow, so that adding an engine component does not move every game's.
inline constexpr ComponentId FIRST_GAME_COMPONENT = 32;
static_assert(ENGINE_COMPONENT_COUNT <= FIRST_GAME_COMPONENT, "the engine's components have grown into the ids kept for games -- raise FIRST_GAME_COMPONENT");
using ComponentMask = std::bitset<MAX_COMPONENTS>;
/// @brief What makes a type a component: an id, and a name to save it under.
///
/// Left undefined, so that a type nobody declared is a compile error wherever it is used as a
/// component. It used to be a run-time lookup by `type_index` that answered "Transform" for anything
/// it did not know, which turned a typo in a query into a query over transforms.
///
/// The engine declares its own in `component.hpp`, and a game declares its with `KUKI_COMPONENT`.
template <typename T>
struct ComponentTraits;
template <typename T>
concept IsComponent = requires {
  { ComponentTraits<T>::id } -> std::convertible_to<ComponentId>;
  { ComponentTraits<T>::name } -> std::convertible_to<std::string_view>;
};
template <IsComponent T>
inline constexpr ComponentId ComponentIdOf = ComponentTraits<T>::id;
/// @brief The signature bits of a set of components, as a number the compiler can compare.
///
/// A number and not a `ComponentMask` because `std::bitset` is only `constexpr` from C++23's library
/// onwards, and not every standard library this builds against has caught up. `ComponentMask` takes
/// one in its constructor, which has always been `constexpr`.
template <IsComponent... Cs>
inline constexpr uint64_t MaskBitsOf = ((uint64_t{1} << ComponentIdOf<Cs>) | ... | uint64_t{0});
template <typename List>
inline constexpr uint64_t ListMaskBits = 0;
template <IsComponent... Cs>
inline constexpr uint64_t ListMaskBits<TypeList<Cs...>> = MaskBitsOf<Cs...>;
template <IsComponent... Cs>
constexpr auto MaskOf() -> ComponentMask {
  return ComponentMask(MaskBitsOf<Cs...>);
}
/// @brief Whether no two of the listed components share an id.
template <typename List>
inline constexpr bool HasDistinctIds = true;
template <IsComponent C, IsComponent... Cs>
inline constexpr bool HasDistinctIds<TypeList<C, Cs...>> = ((ComponentIdOf<C> != ComponentIdOf<Cs>) && ...) && HasDistinctIds<TypeList<Cs...>>;
// The engine's own, declared ahead of their definitions so that naming a component in an archetype
// costs a header this small and not one that includes every component there is.
KUKI_ENGINE_COMPONENT(Animator)
KUKI_ENGINE_COMPONENT(AntiAliasing)
KUKI_ENGINE_COMPONENT(AtmosphereSky)
KUKI_ENGINE_COMPONENT(BoundingBox)
KUKI_ENGINE_COMPONENT(Camera)
KUKI_ENGINE_COMPONENT(IndirectLighting)
KUKI_ENGINE_COMPONENT(Light)
KUKI_ENGINE_COMPONENT(MaterialHandle)
KUKI_ENGINE_COMPONENT(MeshHandle)
KUKI_ENGINE_COMPONENT(ModelMaterialHandle)
KUKI_ENGINE_COMPONENT(ModelMeshHandle)
KUKI_ENGINE_COMPONENT(ParticleEmitter)
KUKI_ENGINE_COMPONENT(RenderBinding)
KUKI_ENGINE_COMPONENT(Skeleton)
KUKI_ENGINE_COMPONENT(SkyboxHandle)
KUKI_ENGINE_COMPONENT(TextureHandle)
KUKI_ENGINE_COMPONENT(Transform)
KUKI_ENGINE_COMPONENT(VolumetricClouds)
KUKI_ENGINE_COMPONENT(VolumetricFog)
/// @brief Every engine component, in the order of `ComponentType`.
///
/// What the run-time questions about a component -- its name, a pointer to it as a variant -- are
/// generated from, so that a component added to the enum and declared above is found everywhere
/// those are asked without a switch to extend. `component.hpp` checks this list against the enum.
using EngineComponents = TypeList<Animator, AntiAliasing, AtmosphereSky, BoundingBox, Camera, IndirectLighting, Light, MaterialHandle, MeshHandle, ModelMaterialHandle, ModelMeshHandle, ParticleEmitter, RenderBinding, Skeleton, SkyboxHandle, TextureHandle, Transform, VolumetricClouds, VolumetricFog>;
static_assert(EngineComponents::size == ENGINE_COMPONENT_COUNT, "a component is in ComponentType but not in EngineComponents, or the other way round");
template <typename List>
inline constexpr bool InIdOrder = false;
template <IsComponent... Cs>
inline constexpr bool InIdOrder<TypeList<Cs...>> = [] {
  ComponentId expected = 0;
  auto ordered = true;
  ((ordered = ordered && ComponentIdOf<Cs> == expected++), ...);
  return ordered;
}();
static_assert(InIdOrder<EngineComponents>, "EngineComponents must list the components in the order of ComponentType, which is what lets an id index it");
} // namespace kuki
