#define GLM_ENABLE_EXPERIMENTAL
#include <application.hpp>
#include <render_system.hpp>
#include <iostream>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <component/camera.hpp>
#include <component/component.hpp>
#include <component/light.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/mesh_filter.hpp>
#include <component/mesh_renderer.hpp>
#include <component/texture.hpp>
#include <component/transform.hpp>
#include <glad/glad.h>
#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtx/quaternion.hpp>
int RenderSystem::RenderSceneToTexture(Camera* camera) {
  auto sceneCamera = app.GetActiveCamera();
  targetCamera = camera ? camera : sceneCamera;
  if (!targetCamera)
    return -1;
  auto& config = app.GetConfig();
  auto width = config.screenWidth;
  auto height = config.screenHeight;
  targetCamera->SetProperty({"AspectRatio", static_cast<float>(width) / height});
  auto multiBufferComplete = UpdateBuffers(sceneMultiFBO, sceneMultiRBO, sceneMultiTexture, width, height, 4);
  auto singleBufferComplete = UpdateBuffers(sceneFBO, sceneRBO, sceneTexture, width, height);
  if (!multiBufferComplete || !singleBufferComplete) {
    std::cerr << "Failed to update framebuffers for scene rendering." << std::endl;
    return -1;
  }
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
void RenderSystem::DrawScene() {
  std::unordered_map<unsigned int, std::unordered_map<std::type_index, std::vector<unsigned int>>> vaoToMatToEntities;
  std::unordered_map<unsigned int, Mesh> vaoToMesh;
  auto camera = app.GetActiveCamera();
  if (!camera)
    return;
  app.ForEachVisibleEntity(*camera, [&](unsigned int id) {
    auto [transform, filter, renderer] = app.GetComponents<Transform, MeshFilter, MeshRenderer>(id);
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
void RenderSystem::DrawEntity(const Transform* transform, const Mesh& mesh, const Material& material) {
  auto shader = GetMaterialShader(material);
  shader->Use();
  material.Apply(*shader);
  shader->SetUniform("useInstancing", false);
  auto model = GetEntityWorldTransform(transform);
  shader->SetMVP(model, targetCamera->view, targetCamera->projection);
  shader->SetUniform("viewPos", targetCamera->position);
  auto dirExists = false;
  auto pointCount = 0;
  app.ForEachEntity<Light>([&](unsigned int id, Light* light) {
    shader->SetLight(light, pointCount);
    if (light->type == LightType::Directional)
      dirExists = true;
    else
      pointCount++;
  });
  shader->SetUniform("dirExists", dirExists);
  shader->SetUniform("pointCount", pointCount);
  glBindVertexArray(mesh.vertexArray);
  if (mesh.indexCount > 0)
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
  else
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
}
void RenderSystem::DrawEntitiesInstanced(const Mesh& mesh, const std::vector<unsigned int>& entities) {
  std::vector<Material> materials;
  std::vector<glm::mat4> models;
  for (const auto id : entities) {
    auto [renderer, transform] = app.GetComponents<MeshRenderer, Transform>(id);
    if (!renderer || !transform)
      continue;
    materials.push_back(renderer->material);
    models.push_back(GetEntityWorldTransform(transform));
  }
  if (materials.size() == 0)
    return;
  auto& material = materials[0];
  auto shader = GetMaterialShader(material);
  shader->Use();
  material.Apply(*shader);
  shader->SetUniform("useInstancing", true);
  shader->SetUniform("view", targetCamera->view);
  shader->SetUniform("projection", targetCamera->projection);
  shader->SetUniform("viewPos", targetCamera->position);
  auto dirExists = false;
  auto pointCount = 0;
  app.ForEachEntity<Light>([&](unsigned int id, Light* light) {
    shader->SetLight(light, pointCount);
    if (light->type == LightType::Directional)
      dirExists = true;
    else
      pointCount++;
  });
  shader->SetUniform("dirExists", dirExists);
  shader->SetUniform("pointCount", pointCount);
  glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
  glBufferData(GL_ARRAY_BUFFER, models.size() * sizeof(glm::mat4), models.data(), GL_DYNAMIC_DRAW);
  glBindVertexArray(mesh.vertexArray);
  unsigned int attribLocation = 4; // 0: i_position, 1: i_normal, 2: i_texCoord, 3: i_tangent
  for (auto i = 0; i < 4; ++i) {
    glEnableVertexAttribArray(attribLocation + i);
    glVertexAttribPointer(attribLocation + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4) * i));
    glVertexAttribDivisor(attribLocation + i, 1);
  }
  if (mesh.indexCount > 0)
    glDrawElementsInstanced(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0, models.size());
  else
    glDrawArraysInstanced(GL_TRIANGLES, 0, mesh.vertexCount, models.size());
}
void RenderSystem::DrawSkybox() {
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
glm::mat4 RenderSystem::GetEntityWorldTransform(const Transform* transform) {
  //if (transform->dirty) {
  auto translation = glm::translate(glm::mat4(1.0f), transform->position);
  auto rotation = glm::toMat4(transform->rotation);
  auto scale = glm::scale(glm::mat4(1.0f), transform->scale);
  transform->model = translation * rotation * scale;
  //transform->dirty = false;
  //}
  if (transform->parent >= 0)
    if (auto parent = app.GetComponent<Transform>(transform->parent))
      transform->model = GetEntityWorldTransform(parent) * transform->model;
  return transform->model;
}
