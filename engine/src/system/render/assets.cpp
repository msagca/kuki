#define GLM_ENABLE_EXPERIMENTAL
#include <application.hpp>
#include <component/component.hpp>
#include <component/light.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/texture.hpp>
#include <component/transform.hpp>
#include <functional>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtx/quaternion.hpp>
#include <render_system.hpp>
#include <utility>
#include <variant>
#include <vector>
int RenderSystem::RenderAssetToTexture(unsigned int assetId, int size) {
  auto isTexture = app.AssetHasComponent<Texture>(assetId);
  auto isCubeMap = false;
  if (assetToTexture.find(assetId) == assetToTexture.end()) {
    if (isTexture) {
      auto texture = app.GetAssetComponent<Texture>(assetId);
      isCubeMap = texture->type == TextureType::CubeMap;
      if (!isCubeMap)
        assetToTexture[assetId] = texture->id;
    }
    if (!isTexture || isCubeMap) {
      auto itemId = texturePool.Request();
      assetToTexture[assetId] = *texturePool.Get(itemId);
    }
  }
  auto textureId = assetToTexture[assetId]; // NOTE: this is the target texture that the asset will be rendered to
  if (!isTexture || isCubeMap) { // NOTE: for textures that are not cubemaps, we can directly use the OpenGL texture IDs
    size = std::max(1, size);
    UpdateBuffers(assetFBO, assetRBO, textureId, size, size, true);
    glBindFramebuffer(GL_FRAMEBUFFER, assetFBO);
    glViewport(0, 0, size, size);
    glClearColor(.0f, .0f, .0f, .0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    if (isCubeMap)
      DrawSkyboxFlat(assetId);
    else
      DrawAsset(assetId);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  return textureId;
}
void RenderSystem::DrawSkyboxFlat(unsigned int id) {
  auto texture = app.GetAssetComponent<Texture>(id);
  if (!texture)
    return;
  auto frameId = app.GetAssetId("Frame");
  auto mesh = app.GetAssetComponent<Mesh>(frameId);
  if (!mesh)
    return;
  auto shader = shaders["SkyboxFlat"];
  shader->Use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, texture->id);
  shader->SetUniform("skybox", 0);
  glBindVertexArray(mesh->vertexArray);
  if (mesh->indexCount > 0)
    glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
  else
    glDrawArrays(GL_TRIANGLES, 0, mesh->vertexCount);
}
void RenderSystem::DrawAsset(unsigned int id) {
  auto bounds = GetAssetBounds(id);
  assetCam.Frame(bounds);
  auto [transform, mesh, material] = app.GetAssetComponents<Transform, Mesh, Material>(id);
  if (transform && mesh && material)
    DrawAsset(transform, *mesh, *material);
  app.ForEachChildAsset(id, [this](unsigned int childId) {
    DrawAsset(childId);
  });
}
void RenderSystem::DrawAsset(const Transform* transform, const Mesh& mesh, const Material& material) {
  static const Light dirLight;
  static const std::vector<const Light*> lights{&dirLight};
  std::vector<glm::mat4> transforms{GetAssetWorldTransform(transform)};
  std::vector<LitFallbackData> materials;
  auto& litMaterial = std::get<LitMaterial>(material.material);
  materials.push_back(litMaterial.fallback.data);
  auto shader = GetMaterialShader(material);
  shader->Use();
  material.Apply(*shader);
  shader->SetUniform("view", assetCam.view);
  shader->SetUniform("projection", assetCam.projection);
  shader->SetUniform("viewPos", assetCam.position);
  shader->SetLighting(lights);
  shader->SetInstanceData(&mesh, transforms, materials, instanceVBO, materialVBO);
  glBindVertexArray(mesh.vertexArray);
  if (mesh.indexCount > 0)
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
  else
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
  glBindVertexArray(0);
}
glm::mat4 RenderSystem::GetAssetWorldTransform(const Transform* transform) {
  auto translation = glm::translate(glm::mat4(1.0f), transform->position);
  auto rotation = glm::toMat4(transform->rotation);
  auto scale = glm::scale(glm::mat4(1.0f), transform->scale);
  transform->model = translation * rotation * scale;
  if (transform->parent >= 0)
    if (auto parent = app.GetAssetComponent<Transform>(transform->parent))
      transform->model = GetAssetWorldTransform(parent) * transform->model;
  return transform->model;
}
glm::vec3 RenderSystem::GetAssetWorldPosition(const Transform* transform) {
  auto model = GetAssetWorldTransform(transform);
  return model[3];
}
BoundingBox RenderSystem::GetAssetBounds(unsigned int id) {
  BoundingBox bounds;
  std::function<void(unsigned int)> calculateBounds = [&](unsigned int assetId) {
    auto [mesh, transform] = app.GetAssetComponents<Mesh, Transform>(assetId);
    if (mesh && transform) {
      auto childBounds = mesh->bounds.GetWorldBounds(transform);
      bounds.min = glm::min(bounds.min, childBounds.min);
      bounds.max = glm::max(bounds.max, childBounds.max);
    }
    app.ForEachChildAsset(assetId, [&](unsigned int childId) {
      calculateBounds(childId);
    });
  };
  calculateBounds(id);
  return bounds;
}
