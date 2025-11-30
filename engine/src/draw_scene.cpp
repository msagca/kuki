#define GLM_ENABLE_EXPERIMENTAL
#include <application.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <gl_constants.hpp>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <light.hpp>
#include <material.hpp>
#include <mesh.hpp>
#include <mesh_filter.hpp>
#include <mesh_renderer.hpp>
#include <rendering_system.hpp>
#include <shader.hpp>
#include <skybox.hpp>
#include <transform.hpp>
#include <unordered_map>
#include <variant>
#include <vector>
namespace kuki {
void RenderingSystem::DrawScene(const Camera* camera, const Camera* observer) {
  if (!camera)
    return;
  std::unordered_map<GLConst::UINT, std::vector<ID>> vaoToEntities;
  std::unordered_map<GLConst::UINT, Mesh> vaoToMesh;
  // TODO: use ForEachVisibleEntity below
  app.ForEachEntity<MeshFilter>([this, &vaoToMesh, &vaoToEntities](ID id, MeshFilter* filter) {
    auto vao = filter->mesh.vao;
    vaoToMesh[vao] = filter->mesh;
    vaoToEntities[vao].push_back(id);
  });
  if (wireframeMode)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  auto targetCam = observer ? observer : camera;
  for (const auto& [vao, entities] : vaoToEntities)
    DrawEntitiesInstanced(targetCam, &vaoToMesh[vao], entities);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  app.ForFirstEntity<Skybox>([this, &targetCam](ID id, Skybox* skybox) {
    DrawSkybox(targetCam, skybox);
  });
}
void RenderingSystem::DrawSkybox(const Camera* camera, const Skybox* skybox) {
  if (!camera || !skybox || skybox->skybox == 0)
    return;
  auto cubeAsset = app.GetAssetId("CubeInverted");
  auto mesh = app.GetAssetComponent<Mesh>(cubeAsset);
  if (!mesh)
    return;
  auto shader = GetShader(MaterialType::Skybox);
  shader->Use();
  shader->SetCamera(camera);
  auto model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
  shader->SetUniform("model", model);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->skybox);
  shader->SetUniform("skybox", 0);
  glDepthFunc(GL_LEQUAL);
  shader->Draw(mesh);
  glDepthFunc(GL_LESS);
}
void RenderingSystem::DrawEntitiesInstanced(const Camera* camera, const Mesh* mesh, const std::vector<ID>& entities) {
  if (!camera || !mesh)
    return;
  std::vector<LitFallbackData> litMaterials;
  std::vector<UnlitFallbackData> unlitMaterials;
  std::vector<glm::mat4> litTransforms;
  std::vector<glm::mat4> unlitTransforms;
  // NOTE: following variables will store the last instance of that material type
  Material materialLit;
  Material materialUnlit;
  // FIXME: a separate draw call shall be invoked per unique material configuration (e.g., different albedo textures)
  for (const auto& id : entities) {
    auto [transform, renderer] = app.GetEntityComponents<Transform, MeshRenderer>(id);
    if (!renderer)
      continue;
    if (auto litMaterial = std::get_if<LitMaterial>(&materialLit.material)) {
      materialLit = renderer->material;
      litMaterials.push_back(litMaterial->fallback);
      litTransforms.push_back(transform->world);
    } else if (auto unlitMaterial = std::get_if<UnlitMaterial>(&materialUnlit.material)) {
      materialUnlit = renderer->material;
      unlitMaterials.push_back(unlitMaterial->fallback);
      unlitTransforms.push_back(transform->world);
    } // else ...
  }
  if (litMaterials.size() > 0) {
    Skybox* skybox{};
    app.ForFirstEntity<Skybox>([this, &skybox](ID id, Skybox* skyboxComp) {
      skybox = skyboxComp;
    });
    auto shader = static_cast<LitShader*>(GetShader(MaterialType::Lit));
    std::vector<const Light*> lights;
    app.ForEachEntity<Light>([&](ID id, Light* light) {
      lights.push_back(light);
    });
    shader->Use();
    shader->SetCamera(camera);
    shader->SetLighting(lights);
    if (skybox && skybox->skybox != 0) {
      // TODO: let the shader handle which texture unit to use
      glActiveTexture(GL_TEXTURE5); // units 0-4 are used by other textures such as albedo map
      glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->irradiance);
      glActiveTexture(GL_TEXTURE6);
      glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->prefilter);
      glActiveTexture(GL_TEXTURE7);
      glBindTexture(GL_TEXTURE_2D, skybox->brdf);
      shader->SetUniform("irradianceMap", 5);
      shader->SetUniform("prefilterMap", 6);
      shader->SetUniform("brdfLUT", 7);
      shader->SetUniform("hasSkybox", true);
    } else
      shader->SetUniform("hasSkybox", false);
    shader->SetMaterial(&materialLit);
    shader->SetMaterialFallback(mesh, litMaterials, materialVBO);
    shader->SetTransform(mesh, litTransforms, transformVBO);
    shader->DrawInstanced(mesh, litTransforms.size());
  }
  if (unlitMaterials.size() > 0) {
    auto shader = static_cast<UnlitShader*>(GetShader(MaterialType::Unlit));
    shader->Use();
    shader->SetCamera(camera);
    shader->SetMaterial(&materialUnlit);
    shader->SetMaterialFallback(mesh, unlitMaterials, materialVBO);
    shader->SetTransform(mesh, unlitTransforms, transformVBO);
    shader->DrawInstanced(mesh, unlitTransforms.size());
  }
}
} // namespace kuki
