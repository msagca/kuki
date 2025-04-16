#define GLM_ENABLE_EXPERIMENTAL
#include <application.hpp>
#include <component/camera.hpp>
#include <component/light.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/mesh_filter.hpp>
#include <component/mesh_renderer.hpp>
#include <component/texture.hpp>
#include <component/transform.hpp>
#include <glad/glad.h>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtx/quaternion.hpp>
#include <system/rendering.hpp>
#include <typeindex>
#include <unordered_map>
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
  UpdateBuffers(sceneMultiFBO, sceneMultiRBO, sceneMultiTexture, width, height, 4);
  UpdateBuffers(sceneFBO, sceneRBO, sceneTexture, width, height);
  glBindFramebuffer(GL_FRAMEBUFFER, sceneMultiFBO);
  glViewport(0, 0, width, height);
  glClearColor(.0f, .0f, .0f, .0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  DrawScene();
  DrawGizmos();
  glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneMultiFBO);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneFBO);
  glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return sceneTexture;
}
void RenderingSystem::DrawScene() {
  std::unordered_map<unsigned int, std::unordered_map<std::type_index, std::vector<unsigned int>>> vaoToMatToEntities;
  std::unordered_map<unsigned int, Mesh> vaoToMesh;
  auto camera = app.GetActiveCamera();
  if (!camera)
    return;
  app.ForEachVisibleEntity(*camera, [&](unsigned int id) {
    auto [transform, filter, renderer] = app.GetEntityComponents<Transform, MeshFilter, MeshRenderer>(id);
    if (!transform || !filter || !renderer)
      return;
    auto vao = filter->mesh.vertexArray;
    vaoToMesh[vao] = filter->mesh;
    if (vaoToMatToEntities.find(vao) == vaoToMatToEntities.end())
      vaoToMatToEntities[vao] = {};
    auto mat = renderer->material.GetTypeIndex();
    if (vaoToMatToEntities[vao].find(mat) == vaoToMatToEntities[vao].end())
      vaoToMatToEntities[vao][mat] = {};
    vaoToMatToEntities[vao][mat].push_back(id);
  });
  for (const auto& [vao, matToEntities] : vaoToMatToEntities)
    for (const auto& [mat, entities] : matToEntities)
      DrawEntitiesInstanced(vaoToMesh[vao], entities);
  DrawSkybox();
}
void RenderingSystem::DrawEntitiesInstanced(const Mesh& mesh, const std::vector<unsigned int>& entities) {
  std::vector<LitFallbackData> materials;
  std::vector<glm::mat4> transforms;
  LitMaterial material{};
  for (const auto id : entities) {
    auto [renderer, transform] = app.GetEntityComponents<MeshRenderer, Transform>(id);
    if (!renderer || !transform)
      continue;
    // TODO: do not assume a lit material
    auto& litMaterial = std::get<LitMaterial>(renderer->material.material);
    material = litMaterial;
    materials.push_back(litMaterial.fallback.data);
    transforms.push_back(GetEntityWorldTransform(transform));
  }
  if (materials.empty())
    return;
  auto shader = shaders["Lit"];
  shader->Use();
  material.Apply(*shader);
  shader->SetUniform("view", targetCamera->view);
  shader->SetUniform("projection", targetCamera->projection);
  shader->SetUniform("viewPos", targetCamera->position);
  std::vector<const Light*> lights;
  app.ForEachEntity<Light>([&](unsigned int id, Light* light) {
    lights.push_back(light);
  });
  shader->SetLighting(lights);
  shader->SetInstanceData(&mesh, transforms, materials, instanceVBO, materialVBO);
  glBindVertexArray(mesh.vertexArray);
  if (mesh.indexCount > 0)
    glDrawElementsInstanced(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0, transforms.size());
  else
    glDrawArraysInstanced(GL_TRIANGLES, 0, mesh.vertexCount, transforms.size());
  glBindVertexArray(0);
}
void RenderingSystem::DrawSkybox() {
  auto assetId = app.GetAssetId("CubeInverted");
  auto mesh = app.GetAssetComponent<Mesh>(assetId);
  if (!mesh)
    return;
  auto shader = shaders["Skybox"];
  shader->Use();
  auto view = glm::mat4(glm::mat3(targetCamera->view));
  shader->SetUniform("view", view);
  shader->SetUniform("projection", targetCamera->projection);
  assetId = app.GetAssetId("Skybox");
  auto texture = app.GetAssetComponent<Texture>(assetId);
  if (!texture)
    return;
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, texture->id);
  shader->SetUniform("skybox", 0);
  glDepthFunc(GL_LEQUAL);
  glBindVertexArray(mesh->vertexArray);
  if (mesh->indexCount > 0)
    glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
  else
    glDrawArrays(GL_TRIANGLES, 0, mesh->vertexCount);
  glDepthFunc(GL_LESS);
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
