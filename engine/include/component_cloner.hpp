#pragma once
#include <entity_manager.hpp>
#include <kuki_engine_export.h>
namespace kuki {
class KUKI_ENGINE_API ComponentCloner {
public:
  ComponentCloner(EntityManager &, const EntityID);
  auto operator()(BoneData *) -> void;
  auto operator()(Camera *) -> void;
  auto operator()(GLMaterial *) -> void;
  auto operator()(GLMesh *) -> void;
  auto operator()(GLSkybox *) -> void;
  auto operator()(GLTexture *) -> void;
  auto operator()(Light *) -> void;
  auto operator()(MaterialHandle *) -> void;
  auto operator()(MeshHandle *) -> void;
  auto operator()(SkyboxHandle *) -> void;
  auto operator()(TextureHandle *) -> void;
  auto operator()(Transform *) -> void;
private:
  EntityManager &entityManager;
  const EntityID entityId;
};
} // namespace kuki
