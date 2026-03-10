#pragma once
#include <entity_manager.hpp>
#include <kuki_engine_export.h>
namespace kuki {
class KUKI_ENGINE_API ComponentCloner {
public:
  ComponentCloner(EntityManager &, const EntityID);
  auto operator()(const BoneData *) -> void;
  auto operator()(const Camera *) -> void;
  auto operator()(const GLMaterial *) -> void;
  auto operator()(const GLMesh *) -> void;
  auto operator()(const GLSkybox *) -> void;
  auto operator()(const GLTexture *) -> void;
  auto operator()(const Light *) -> void;
  auto operator()(const MaterialHandle *) -> void;
  auto operator()(const MeshHandle *) -> void;
  auto operator()(const SkyboxHandle *) -> void;
  auto operator()(const TextureHandle *) -> void;
  auto operator()(const Transform *) -> void;
  auto operator()(std::monostate) -> void;
private:
  EntityManager &entityManager;
  const EntityID entityId;
};
} // namespace kuki
