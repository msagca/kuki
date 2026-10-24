#pragma once
#include <archetype.hpp>
#include <component_traits.hpp>
#include <type_list.hpp>
namespace kuki {
/// @brief What a mesh asset needs to be drawn: the handles, bounds to cull by, and the backend's binding.
using Renderable = Components<MeshHandle, MaterialHandle, BoundingBox, RenderBinding>;
/// @brief What one of a model's meshes needs to be drawn.
using ModelRenderable = Components<ModelMeshHandle, ModelMaterialHandle, BoundingBox, RenderBinding>;
/// @brief What plays a model's clips: the bones' entities and the animator that poses them.
using Animated = Components<Skeleton, Animator>;
/// @brief Something with a place and nothing else: a group, a pivot, the parent of a clock's parts.
using EmptyNode = Archetype<Transform>;
using MeshObject = Archetype<Transform, Renderable>;
using CameraObject = Archetype<Transform, Camera>;
using LightObject = Archetype<Transform, Light>;
using ParticleObject = Archetype<Transform, ParticleEmitter>;
/// @brief A sky from a texture. `RenderingSystem` adopts a sun from it into a `Light` on the same
/// entity when it has one, so a game that wants that declares the sky with a light of its own.
using SkyboxObject = Archetype<Transform, SkyboxHandle>;
/// @brief The settings a scene has one of, each on an entity of its own. A game that wants several
/// on one entity -- a "sky" that is both the atmosphere and the lighting -- declares that archetype.
using AtmosphereSkyObject = Archetype<Transform, AtmosphereSky>;
using IndirectLightingObject = Archetype<Transform, IndirectLighting>;
using VolumetricCloudsObject = Archetype<Transform, VolumetricClouds>;
using VolumetricFogObject = Archetype<Transform, VolumetricFog>;
using AntiAliasingObject = Archetype<Transform, AntiAliasing>;
/// @brief One node of a placed model that carries no mesh: a bone, or a group of meshes.
using ModelNodeObject = Archetype<Transform, BoundingBox>;
/// @brief One node of a placed model that carries a mesh.
using ModelMeshObject = Archetype<Transform, ModelRenderable>;
/// @brief The entity a model is placed as, unless the game asks for another (`InstantiateAsset<A>`).
///
/// It stands for the model's top node: it takes that node's transform and bounds, and the nodes
/// below come in as its children. A top node that carries a mesh brings it as a child of its own.
using ModelRoot = Archetype<Transform, BoundingBox, Animated>;
/// @brief The archetypes every scene has, whatever its game declares besides.
///
/// What the engine itself makes: the prefabs of models and meshes it places, the entities the
/// builder's conveniences make, and the settings. A game's `World` adds its own to these.
using EngineArchetypes = TypeList<EmptyNode, MeshObject, CameraObject, LightObject, ParticleObject, SkyboxObject, AtmosphereSkyObject, IndirectLightingObject, VolumetricCloudsObject, VolumetricFogObject, AntiAliasingObject, ModelNodeObject, ModelMeshObject, ModelRoot>;
} // namespace kuki
