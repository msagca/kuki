#include <component_cloner.hpp>
namespace kuki {
ComponentCloner::ComponentCloner(EntityManager &entityManager, const EntityID entityId)
  : entityManager(entityManager), entityId(entityId) {}
auto ComponentCloner::operator()(BoneData *other) -> void {
  if (!other)
    return;
  auto boneData = entityManager.AddComponent<BoneData>(entityId);
  *boneData = *other;
}
auto ComponentCloner::operator()(Camera *other) -> void {
  if (!other)
    return;
  auto camera = entityManager.AddComponent<Camera>(entityId);
  *camera = *other;
}
auto ComponentCloner::operator()(GLMaterial *other) -> void {
  if (!other)
    return;
  auto glMaterial = entityManager.AddComponent<GLMaterial>(entityId);
  *glMaterial = *other;
}
auto ComponentCloner::operator()(GLMesh *other) -> void {
  if (!other)
    return;
  auto glMesh = entityManager.AddComponent<GLMesh>(entityId);
  *glMesh = *other;
}
auto ComponentCloner::operator()(GLSkybox *other) -> void {
  if (!other)
    return;
  auto glSkybox = entityManager.AddComponent<GLSkybox>(entityId);
  *glSkybox = *other;
}
auto ComponentCloner::operator()(GLTexture *other) -> void {
  if (!other)
    return;
  auto glTexture = entityManager.AddComponent<GLTexture>(entityId);
  *glTexture = *other;
}
auto ComponentCloner::operator()(Light *other) -> void {
  if (!other)
    return;
  auto light = entityManager.AddComponent<Light>(entityId);
  *light = *other;
}
auto ComponentCloner::operator()(MaterialHandle *other) -> void {
  if (!other)
    return;
  auto materialHandle = entityManager.AddComponent<MaterialHandle>(entityId);
  *materialHandle = *other;
}
auto ComponentCloner::operator()(MeshHandle *other) -> void {
  if (!other)
    return;
  auto meshHandle = entityManager.AddComponent<MeshHandle>(entityId);
  *meshHandle = *other;
}
auto ComponentCloner::operator()(SkyboxHandle *other) -> void {
  if (!other)
    return;
  auto skyboxHandle = entityManager.AddComponent<SkyboxHandle>(entityId);
  *skyboxHandle = *other;
}
auto ComponentCloner::operator()(TextureHandle *other) -> void {
  if (!other)
    return;
  auto textureHandle = entityManager.AddComponent<TextureHandle>(entityId);
  *textureHandle = *other;
}
auto ComponentCloner::operator()(Transform *other) -> void {
  if (!other)
    return;
  auto transform = entityManager.AddComponent<Transform>(entityId);
  *transform = *other;
}
} // namespace kuki
