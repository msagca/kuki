#pragma once
#include <camera.hpp>
#include <component.hpp>
#include <entity_manager.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <rig.hpp>
#include <slot_store.hpp>
namespace kuki {
/// @brief One scene: its entities, which of its cameras is drawn from, and its models' rigs.
///
/// Only what a scene adds to its `EntityManager`. Every entity operation is the manager's, reached
/// through `Entities()`, and is not repeated here: the scene used to forward forty of them under
/// names of its own (`GetEntityName` for `GetName`, `DeleteEntity` for `Delete`), which made two
/// names for each and a second place for every change to land.
///
/// Internal. A game never holds a scene: its entities are reached through its `World` and
/// `Application`, the camera through `Application::GetCamera`, and the scene's singletons through
/// `Application::GetSceneSingleton`.
class KUKI_ENGINE_API Scene {
public:
  Scene(const SceneID);
  const SceneID id;
  auto Entities(this auto &self) -> decltype(auto) {
    return (self.entityManager);
  }
  /// @brief The camera drawn from: the one made active, or else the first there is. Null when the
  /// scene has no camera.
  auto GetActiveCamera(this auto &) -> decltype(auto);
  /// @brief The entity of the camera drawn from, by the same choice. Invalid when there is none.
  auto GetActiveCameraEntity() const -> EntityID;
  /// @brief Makes an entity's camera the one drawn from. False when it has no camera.
  auto SetActiveCamera(const EntityID) -> bool;
  /// @brief The rig of a model's root, through its `Skeleton`. Null for an entity with no skeleton,
  /// or whose skeleton was never given one.
  auto GetRig(const EntityID) -> Rig *;
  auto GetRig(const EntityID) const -> const Rig *;
  /// @brief The rig of a model's root, given one first if it has none. Null for an entity with no skeleton.
  auto ClaimRig(const EntityID) -> Rig *;
  /// @brief Empties the rigs of entities that have gone, for others to take. `RenderingSystem` does
  /// it once a frame; nothing tells the scene when a model is removed.
  auto SweepRigs() -> void;
  /// @brief Removes every entity and empties every rig.
  auto Clear() -> void;
private:
  EntityManager entityManager{};
  EntityID activeCameraId{};
  SlotStore<Rig> rigs;
};
auto Scene::GetActiveCamera(this auto &self) -> decltype(auto) {
  return self.entityManager.template GetComponent<Camera>(self.GetActiveCameraEntity());
}
} // namespace kuki
