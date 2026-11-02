#include <asset_manager.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <component_type.hpp>
#include <entity_manager.hpp>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <optional>
#include <scene.hpp>
#include <string>
#include <transform.hpp>
#include <utility>
#include <vector>
namespace kuki {
Scene::Scene(const SceneID id)
  : id(id) {}
auto Scene::Clear() -> void {
  entityManager.Clear();
  rigs.Clear();
}
auto Scene::GetRig(const EntityID id) -> Rig * {
  const auto *skeleton = entityManager.GetComponent<Skeleton>(id);
  return skeleton ? rigs.Find(skeleton->rig, id) : nullptr;
}
auto Scene::GetRig(const EntityID id) const -> const Rig * {
  const auto *skeleton = entityManager.GetComponent<Skeleton>(id);
  return skeleton ? rigs.Find(skeleton->rig, id) : nullptr;
}
auto Scene::ClaimRig(const EntityID id) -> Rig * {
  auto *skeleton = entityManager.GetComponent<Skeleton>(id);
  return skeleton ? &rigs.Claim(skeleton->rig, id) : nullptr;
}
auto Scene::SweepRigs() -> void {
  rigs.Sweep([this](const EntityID id) { return entityManager.IsEntity(id); });
}
auto Scene::GetActiveCameraEntity() const -> EntityID {
  if (activeCameraId && entityManager.HasComponent<Camera>(activeCameraId))
    return activeCameraId;
  auto first = EntityID::Invalid;
  entityManager.ForEach<Camera>([&](const EntityID id, const Camera *) {
    if (!first)
      first = id;
  });
  return first;
}
auto Scene::SetActiveCamera(const EntityID id) -> bool {
  if (!entityManager.HasComponent<Camera>(id))
    return false;
  activeCameraId = id;
  return true;
}
} // namespace kuki
