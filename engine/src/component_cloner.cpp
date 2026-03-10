#include <component_cloner.hpp>
namespace kuki {
ComponentCloner::ComponentCloner(EntityManager &entityManager, const EntityID entityId)
  : entityManager(entityManager), entityId(entityId) {}
auto ComponentCloner::operator()(const BoneData *other) -> void {
  if (!other)
    return;
  auto boneData = entityManager.AddComponent<BoneData>(entityId);
  *boneData = *other;
}
auto ComponentCloner::operator()(const Camera *other) -> void {
  if (!other)
    return;
  auto camera = entityManager.AddComponent<Camera>(entityId);
  *camera = *other;
}
auto ComponentCloner::operator()(const GLMaterial *other) -> void {
  if (!other)
    return;
  auto glMaterial = entityManager.AddComponent<GLMaterial>(entityId);
  *glMaterial = *other;
}
auto ComponentCloner::operator()(const GLMesh *other) -> void {
  if (!other)
    return;
  auto glMesh = entityManager.AddComponent<GLMesh>(entityId);
  *glMesh = *other;
}
auto ComponentCloner::operator()(const GLSkybox *other) -> void {
  if (!other)
    return;
  auto glSkybox = entityManager.AddComponent<GLSkybox>(entityId);
  *glSkybox = *other;
}
auto ComponentCloner::operator()(const GLTexture *other) -> void {
  if (!other)
    return;
  auto glTexture = entityManager.AddComponent<GLTexture>(entityId);
  *glTexture = *other;
}
auto ComponentCloner::operator()(const Light *other) -> void {
  if (!other)
    return;
  auto light = entityManager.AddComponent<Light>(entityId);
  *light = *other;
}
auto ComponentCloner::operator()(const MaterialHandle *other) -> void {
  if (!other)
    return;
  auto materialHandle = entityManager.AddComponent<MaterialHandle>(entityId);
  *materialHandle = *other;
}
auto ComponentCloner::operator()(const MeshHandle *other) -> void {
  if (!other)
    return;
  auto meshHandle = entityManager.AddComponent<MeshHandle>(entityId);
  *meshHandle = *other;
}
auto ComponentCloner::operator()(const SkyboxHandle *other) -> void {
  if (!other)
    return;
  auto skyboxHandle = entityManager.AddComponent<SkyboxHandle>(entityId);
  *skyboxHandle = *other;
}
auto ComponentCloner::operator()(const TextureHandle *other) -> void {
  if (!other)
    return;
  auto textureHandle = entityManager.AddComponent<TextureHandle>(entityId);
  *textureHandle = *other;
}
auto ComponentCloner::operator()(const Transform *other) -> void {
  if (!other)
    return;
  auto transform = entityManager.AddComponent<Transform>(entityId);
  *transform = *other;
}
auto ComponentCloner::operator()(const std::monostate) -> void {}
} // namespace kuki
