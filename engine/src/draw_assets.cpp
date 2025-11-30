#include <application.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <glm/detail/type_vec3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <id.hpp>
#include <light.hpp>
#include <material.hpp>
#include <mesh.hpp>
#include <rendering_system.hpp>
#include <shader.hpp>
#include <skybox.hpp>
#include <transform.hpp>
#include <variant>
namespace kuki {
void RenderingSystem::DrawSkyboxAsset(ID id) {
  auto skybox = app.GetAssetComponent<Skybox>(id);
  if (!skybox || skybox->skybox == 0)
    return;
  auto frameId = app.GetAssetId("Frame");
  auto mesh = app.GetAssetComponent<Mesh>(frameId);
  if (!mesh)
    return;
  static Material material;
  material.material = UnlitMaterial();
  auto& unlitMaterial = std::get<UnlitMaterial>(material.material);
  unlitMaterial.data.base = skybox->skybox;
  unlitMaterial.type = MaterialType::CubeMapEquirect;
  auto shader = GetShader(MaterialType::CubeMapEquirect);
  shader->Use();
  shader->SetUniform("model", glm::mat4(1.0));
  shader->SetMaterial(&material);
  shader->Draw(mesh);
}
void RenderingSystem::DrawAssetHierarchy(ID id) {
  static const Light dirLight{};
  auto shader = static_cast<LitShader*>(GetShader(MaterialType::Lit));
  auto bounds = GetAssetBounds(id);
  assetCam.Frame(bounds);
  shader->Use();
  shader->SetCamera(&assetCam);
  shader->SetLighting(&dirLight);
  DrawAsset(id);
}
void RenderingSystem::DrawAsset(ID id) {
  auto [transform, mesh, material] = app.GetAssetComponents<Transform, Mesh, Material>(id);
  if (transform && mesh && material) {
    auto model = transform->world;
    auto shader = static_cast<LitShader*>(GetShader(MaterialType::Lit));
    shader->SetMaterial(material);
    if (auto litMaterial = std::get_if<LitMaterial>(&material->material))
      // TODO: support other materials
      shader->SetMaterialFallback(mesh, litMaterial->fallback, materialVBO);
    shader->SetTransform(mesh, model, transformVBO);
    shader->Draw(mesh);
  }
  app.ForEachChildAsset(id, [this](ID childId) {
    DrawAsset(childId);
  });
}
BoundingBox RenderingSystem::GetAssetBounds(ID id) {
  BoundingBox bounds{};
  auto [mesh, transform] = app.GetAssetComponents<Mesh, Transform>(id);
  if (transform && mesh)
    bounds = mesh->bounds.GetWorldBounds(transform->world);
  app.ForEachChildAsset(id, [&](ID childId) {
    auto childBounds = GetAssetBounds(childId);
    bounds.min = glm::min(bounds.min, childBounds.min);
    bounds.max = glm::max(bounds.max, childBounds.max);
  });
  return bounds;
}
} // namespace kuki
