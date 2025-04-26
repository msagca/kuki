#define GLM_ENABLE_EXPERIMENTAL
#include <glad/glad.h>
#include <application.hpp>
#include <component/component.hpp>
#include <component/light.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/texture.hpp>
#include <component/transform.hpp>
#include <functional>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtx/quaternion.hpp>
#include <spdlog/spdlog.h>
#include <system/rendering.hpp>
#include <vector>
#include <variant>
namespace kuki {
int RenderingSystem::RenderAssetToTexture(unsigned int assetId, int size) {
  auto isTexture = app.AssetHasComponent<Texture>(assetId);
  auto isCubeMap = false;
  auto isPresent = assetToTexture.find(assetId) != assetToTexture.end();
  if (!isPresent) {
    if (isTexture) {
      auto texture = app.GetAssetComponent<Texture>(assetId);
      isCubeMap = texture->type == TextureType::CubeMap;
      if (!isCubeMap) // NOTE: for textures that are not cubemaps, we can directly use the OpenGL texture IDs
        assetToTexture[assetId] = texture->id;
    }
    if (!isTexture || isCubeMap) {
      auto itemId = texturePool.Request();
      assetToTexture[assetId] = *texturePool.Get(itemId);
    }
  }
  auto textureId = assetToTexture[assetId]; // NOTE: this is the target texture that the asset will be rendered to
  if (!isPresent && (!isTexture || isCubeMap)) {
    size = std::max(1, size);
    UpdateAttachments(framebuffer, renderbufferAsset, textureId, size, size, true);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
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
void RenderingSystem::DrawSkyboxFlat(unsigned int id) {
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
void RenderingSystem::DrawAsset(unsigned int id) {
  auto bounds = GetAssetBounds(id);
  assetCam.Frame(bounds);
  auto [transform, mesh, material] = app.GetAssetComponents<Transform, Mesh, Material>(id);
  if (transform && mesh && material)
    DrawAsset(transform, *mesh, *material);
  // TODO: these recursive calls cause a lot of overhead, flatten the asset hierarchy
  app.ForEachChildAsset(id, [this](unsigned int childId) {
    DrawAsset(childId);
  });
}
void RenderingSystem::DrawAsset(const Transform* transform, const Mesh& mesh, const Material& material) {
  static const Light dirLight;
  static const std::vector<const Light*> lights{&dirLight};
  std::vector<glm::mat4> transforms{GetAssetWorldTransform(transform)};
  std::vector<LitFallbackData> materials;
  auto& litMaterial = std::get<LitMaterial>(material.material);
  materials.push_back(litMaterial.fallback);
  auto shader = static_cast<LitShader*>(shaders["Lit"]);
  shader->Use();
  material.Apply(*shader);
  shader->SetUniform("view", assetCam.view);
  shader->SetUniform("projection", assetCam.projection);
  shader->SetUniform("viewPos", assetCam.position);
  shader->SetLighting(lights);
  shader->SetInstanceData(&mesh, transforms, materials, transformVBO, materialVBO);
  glBindVertexArray(mesh.vertexArray);
  if (mesh.indexCount > 0)
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
  else
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
  glBindVertexArray(0);
}
int RenderingSystem::RenderRadianceToCubeMap(unsigned int radianceAssetId) {
  auto radianceTexture = app.GetAssetComponent<Texture>(radianceAssetId);
  auto width = 512;
  auto height = 512;
  unsigned int cubeMapTextureId;
  glGenTextures(1, &cubeMapTextureId);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapTextureId);
  for (auto i = 0; i < 6; ++i)
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  auto projection = glm::perspective(glm::radians(90.0f), 1.0f, .1f, 10.0f);
  glm::mat4 viewMatrices[] = {glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(1.0f, .0f, .0f), glm::vec3(.0f, -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(-1.0f, .0f, .0f), glm::vec3(.0f, -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, 1.0f, .0f), glm::vec3(.0f, .0f, 1.0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, -1.0f, .0f), glm::vec3(.0f, .0f, -1.0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, .0f, 1.0f), glm::vec3(.0f, -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, .0f, -1.0f), glm::vec3(.0f, -1.0f, .0f))};
  auto shader = shaders["Radiance"];
  shader->Use();
  shader->SetUniform("equirectangularMap", 0);
  shader->SetUniform("projection", projection);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, radianceTexture->id);
  glViewport(0, 0, width, height);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  auto cubeAssetId = app.GetAssetId("Cube");
  auto cubeMesh = app.GetAssetComponent<Mesh>(cubeAssetId);
  if (!cubeMesh) {
    spdlog::warn("Cube mesh not found. Cannot render radiance to cube map.");
    return -1;
  }
  for (auto i = 0; i < 6; ++i) {
    shader->SetUniform("view", viewMatrices[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubeMapTextureId, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindVertexArray(cubeMesh->vertexArray);
    if (cubeMesh->indexCount > 0)
      glDrawElements(GL_TRIANGLES, cubeMesh->indexCount, GL_UNSIGNED_INT, 0);
    else
      glDrawArrays(GL_TRIANGLES, 0, cubeMesh->vertexCount);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  std::string name = app.GetAssetName(radianceAssetId) + "CubeMap";
  auto cubeMapAssetId = app.CreateAsset(name);
  auto cubeMapTexture = app.AddAssetComponent<Texture>(cubeMapAssetId);
  cubeMapTexture->id = cubeMapTextureId;
  cubeMapTexture->type = TextureType::CubeMap;
  spdlog::info("Radiance HDR cube map is created: {}.", name);
  return cubeMapAssetId;
}
glm::mat4 RenderingSystem::GetAssetWorldTransform(const Transform* transform) {
  auto translation = glm::translate(glm::mat4(1.0f), transform->position);
  auto rotation = glm::toMat4(transform->rotation);
  auto scale = glm::scale(glm::mat4(1.0f), transform->scale);
  transform->model = translation * rotation * scale;
  if (transform->parent >= 0)
    if (auto parent = app.GetAssetComponent<Transform>(transform->parent))
      transform->model = GetAssetWorldTransform(parent) * transform->model;
  return transform->model;
}
glm::vec3 RenderingSystem::GetAssetWorldPosition(const Transform* transform) {
  auto model = GetAssetWorldTransform(transform);
  return model[3];
}
BoundingBox RenderingSystem::GetAssetBounds(unsigned int id) {
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
} // namespace kuki
