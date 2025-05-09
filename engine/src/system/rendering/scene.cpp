#define GLM_ENABLE_EXPERIMENTAL
#include <glad/glad.h>
#include <application.hpp>
#include <component/camera.hpp>
#include <component/component.hpp>
#include <component/light.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/mesh_filter.hpp>
#include <component/mesh_renderer.hpp>
#include <component/shader.hpp>
#include <component/texture.hpp>
#include <component/transform.hpp>
#include <component/skybox.hpp>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtx/quaternion.hpp>
#include <system/rendering.hpp>
#include <unordered_map>
#include <variant>
#include <vector>
namespace kuki {
int RenderingSystem::RenderSceneToTexture(Camera* camera) {
  auto sceneCamera = app.GetActiveCamera();
  targetCamera = camera ? camera : sceneCamera;
  if (!targetCamera)
    return -1;
  auto& config = app.GetConfig();
  auto width = config.screenWidth;
  auto height = config.screenHeight;
  targetCamera->SetProperty({"AspectRatio", static_cast<float>(width) / height});
  UpdateAttachments(framebufferMulti, renderbufferSceneMulti, textureSceneMulti, width, height, 4);
  UpdateAttachments(framebuffer, renderbufferScene, textureScene, width, height);
  glBindFramebuffer(GL_FRAMEBUFFER, framebufferMulti);
  glViewport(0, 0, width, height);
  glClearColor(.0f, .0f, .0f, .0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  DrawScene();
  DrawGizmos();
  glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferMulti);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
  glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return textureScene;
}
void RenderingSystem::DrawScene() {
  std::unordered_map<unsigned int, std::vector<unsigned int>> vaoToEntities;
  std::unordered_map<unsigned int, Mesh> vaoToMesh;
  // FIXME: app.ForEachVisibleEntity does not work as expected
  // TODO: repeat this query only after entity addition/deletion or component updates (set flags in entity manager and check them each frame)
  app.ForEachEntity<MeshFilter, MeshRenderer>([this, &vaoToMesh, &vaoToEntities](unsigned int id, MeshFilter* filter, MeshRenderer* renderer) {
    if (!filter || !renderer)
      return;
    auto vao = filter->mesh.vertexArray;
    vaoToMesh[vao] = filter->mesh;
    if (vaoToEntities.find(vao) == vaoToEntities.end())
      vaoToEntities[vao] = {};
    vaoToEntities[vao].push_back(id);
  });
  for (const auto& [vao, entities] : vaoToEntities)
    DrawEntitiesInstanced(&vaoToMesh[vao], entities);
  app.ForFirstEntity<Skybox>([this](unsigned int id, Skybox* skybox) {
    DrawSkybox(skybox);
  });
}
void RenderingSystem::DrawSkybox(const Skybox* skybox) {
  if (!skybox)
    return;
  auto cubeAsset = app.GetAssetId("CubeInverted");
  auto mesh = app.GetAssetComponent<Mesh>(cubeAsset);
  if (!mesh)
    return;
  auto shader = GetShader(MaterialType::Skybox);
  shader->Use();
  auto model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
  shader->SetUniform("model", model);
  auto view = glm::mat4(glm::mat3(targetCamera->view));
  shader->SetUniform("view", view);
  shader->SetUniform("projection", targetCamera->projection);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->id.x);
  shader->SetUniform("skybox", 0);
  glDepthFunc(GL_LEQUAL);
  shader->Draw(mesh);
  glDepthFunc(GL_LESS);
}
void RenderingSystem::DrawEntitiesInstanced(const Mesh* mesh, const std::vector<unsigned int>& entities) {
  std::vector<LitFallbackData> litMaterials;
  std::vector<UnlitFallbackData> unlitMaterials;
  std::vector<glm::mat4> litTransforms;
  std::vector<glm::mat4> unlitTransforms;
  // NOTE: following variables will store the last instance of that material type
  Material materialLit;
  Material materialUnlit;
  // TODO: a separate draw call shall be invoked per unique material configuration (e.g. different albedo textures)
  for (const auto id : entities) {
    auto [renderer, transform] = app.GetEntityComponents<MeshRenderer, Transform>(id);
    if (!renderer || !transform)
      continue;
    if (renderer->material.GetType() == MaterialType::Lit) {
      materialLit = renderer->material;
      auto& litMaterial = std::get<LitMaterial>(materialLit.material);
      litMaterials.push_back(litMaterial.fallback);
      litTransforms.push_back(GetEntityWorldTransform(transform));
    } else if (renderer->material.GetType() == MaterialType::Unlit) {
      materialUnlit = renderer->material;
      auto& unlitMaterial = std::get<UnlitMaterial>(materialUnlit.material);
      unlitMaterials.push_back(unlitMaterial.fallback);
      unlitTransforms.push_back(GetEntityWorldTransform(transform));
    } // else ...
  }
  if (litMaterials.size() > 0) {
    Skybox* skybox{};
    app.ForFirstEntity<Skybox>([this, &skybox](unsigned int id, Skybox* skyboxComp) {
      skybox = skyboxComp;
    });
    auto shader = static_cast<LitShader*>(GetShader(MaterialType::Lit));
    std::vector<const Light*> lights;
    app.ForEachEntity<Light>([&](unsigned int id, Light* light) {
      lights.push_back(light);
    });
    shader->Use();
    shader->SetCamera(targetCamera);
    shader->SetLighting(lights);
    if (skybox) {
      // TODO: let the shader handle which texture unit to use
      glActiveTexture(GL_TEXTURE5); // units 0-4 are used by other textures such as albedo map
      glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->id.y);
      shader->SetUniform("irradianceMap", 5);
      shader->SetUniform("useIrradianceMap", true);
    } else
      shader->SetUniform("useIrradianceMap", false);
    shader->SetMaterial(&materialLit);
    shader->SetMaterialFallback(mesh, litMaterials, materialVBO);
    shader->SetTransform(mesh, litTransforms, transformVBO);
    shader->DrawInstanced(mesh, litTransforms.size());
  }
  if (unlitMaterials.size() > 0) {
    auto shader = static_cast<UnlitShader*>(GetShader(MaterialType::Unlit));
    shader->Use();
    shader->SetCamera(targetCamera);
    shader->SetMaterial(&materialUnlit);
    shader->SetMaterialFallback(mesh, unlitMaterials, materialVBO);
    shader->SetTransform(mesh, unlitTransforms, transformVBO);
    shader->DrawInstanced(mesh, unlitTransforms.size());
  }
}
glm::mat4 RenderingSystem::GetEntityWorldTransform(const Transform* transform) {
  //if (transform->dirty) {
  auto translation = glm::translate(glm::mat4(1.0f), transform->position);
  auto rotation = glm::toMat4(transform->rotation);
  auto scale = glm::scale(glm::mat4(1.0f), transform->scale);
  transform->model = translation * rotation * scale;
  //transform->dirty = false;
  //}
  if (transform->parent >= 0)
    if (auto parent = app.GetEntityComponent<Transform>(transform->parent))
      transform->model = GetEntityWorldTransform(parent) * transform->model;
  return transform->model;
}
} // namespace kuki
