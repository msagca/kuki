#define GLM_ENABLE_EXPERIMENTAL
#include <application.hpp>
#include <component/component.hpp>
#include <component/light.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/texture.hpp>
#include <component/transform.hpp>
#include <cstddef>
#include <functional>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include <render_system.hpp>
#include <unordered_map>
#include <utility>
#include <vector>
#include <variant>
int RenderSystem::RenderAssetToTexture(unsigned int assetId, int size) {
  static std::unordered_map<unsigned int, unsigned int> assetToTexture;
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
    auto bufferComplete = UpdateBuffers(assetFBO, assetRBO, textureId, size, size);
    if (!bufferComplete) {
      std::cerr << "Failed to update framebuffer for asset rendering." << std::endl;
      return -1;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, assetFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0);
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
  std::vector<glm::mat4> models{GetAssetWorldTransform(transform)};
  std::vector<LitFallback> materials;
  auto& litMaterial = std::get<LitMaterial>(material.material);
  materials.push_back(litMaterial.fallback);
  auto shader = GetMaterialShader(material);
  shader->Use();
  material.Apply(*shader);
  shader->SetUniform("view", assetCam.view);
  shader->SetUniform("projection", assetCam.projection);
  shader->SetUniform("viewPos", assetCam.position);
  static const Light dirLight;
  shader->SetLight(&dirLight);
  shader->SetUniform("dirExists", true);
  shader->SetUniform("pointCount", 0); // NOTE: without this line, the lighting will be impacted by (previously set) point lights in the scene
  glNamedBufferData(instanceVBO, models.size() * sizeof(glm::mat4), models.data(), GL_DYNAMIC_DRAW);
  auto bindingIndex = 0;
  glVertexArrayVertexBuffer(mesh.vertexArray, bindingIndex, instanceVBO, 0, sizeof(glm::mat4));
  for (auto i = 0; i < 4; ++i) {
    auto attrib = 4 + i;
    glVertexArrayAttribFormat(mesh.vertexArray, attrib, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * i);
    glVertexArrayAttribBinding(mesh.vertexArray, attrib, bindingIndex);
    glEnableVertexArrayAttrib(mesh.vertexArray, attrib);
  }
  glVertexArrayBindingDivisor(mesh.vertexArray, bindingIndex, 1);
  bindingIndex++;
  glNamedBufferData(materialVBO, materials.size() * sizeof(LitFallback), materials.data(), GL_DYNAMIC_DRAW);
  glVertexArrayVertexBuffer(mesh.vertexArray, bindingIndex, materialVBO, 0, sizeof(LitFallback));
  glVertexArrayAttribFormat(mesh.vertexArray, 8, 3, GL_FLOAT, GL_FALSE, offsetof(LitFallback, albedo));
  glVertexArrayAttribBinding(mesh.vertexArray, 8, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vertexArray, 8);
  glVertexArrayAttribFormat(mesh.vertexArray, 9, 1, GL_FLOAT, GL_FALSE, offsetof(LitFallback, metalness));
  glVertexArrayAttribBinding(mesh.vertexArray, 9, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vertexArray, 9);
  glVertexArrayAttribFormat(mesh.vertexArray, 10, 1, GL_FLOAT, GL_FALSE, offsetof(LitFallback, roughness));
  glVertexArrayAttribBinding(mesh.vertexArray, 10, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vertexArray, 10);
  glVertexArrayAttribFormat(mesh.vertexArray, 11, 1, GL_FLOAT, GL_FALSE, offsetof(LitFallback, occlusion));
  glVertexArrayAttribBinding(mesh.vertexArray, 11, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vertexArray, 11);
  glBindVertexArray(mesh.vertexArray);
  if (mesh.indexCount > 0)
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
  else
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
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
