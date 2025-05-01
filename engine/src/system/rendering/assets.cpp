#define GLM_ENABLE_EXPERIMENTAL
#include <glad/glad.h>
#include <application.hpp>
#include <component/component.hpp>
#include <component/light.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/shader.hpp>
#include <component/texture.hpp>
#include <component/transform.hpp>
#include <functional>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtx/quaternion.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <system/rendering.hpp>
#include <vector>
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
  static UnlitMaterial material;
  auto texture = app.GetAssetComponent<Texture>(id);
  if (!texture)
    return;
  material.data.base = texture->id;
  material.type = MaterialType::CubeMapEquirect;
  auto frameId = app.GetAssetId("Frame");
  auto mesh = app.GetAssetComponent<Mesh>(frameId);
  if (!mesh)
    return;
  auto shader = GetShader(MaterialType::CubeMapEquirect);
  shader->Use();
  shader->SetMaterial(&material);
  shader->Draw(mesh);
}
void RenderingSystem::DrawAsset(unsigned int id) {
  static const Light dirLight;
  static const std::vector<const Light*> lights{&dirLight};
  auto shader = static_cast<LitShader*>(GetShader(MaterialType::Lit));
  auto bounds = GetAssetBounds(id);
  assetCam.Frame(bounds);
  shader->Use();
  shader->SetCamera(&assetCam);
  shader->SetLighting(lights);
  auto [transform, mesh, material] = app.GetAssetComponents<Transform, Mesh, Material>(id);
  if (transform && mesh && material)
    DrawAsset(transform, mesh, material);
  // TODO: these recursive calls cause a lot of overhead, flatten the asset hierarchy
  app.ForEachChildAsset(id, [this](unsigned int childId) {
    DrawAsset(childId);
  });
}
void RenderingSystem::DrawAsset(const Transform* transform, const Mesh* mesh, const Material* material) {
  auto shader = static_cast<LitShader*>(GetShader(MaterialType::Lit));
  auto& litMaterial = std::get<LitMaterial>(material->material);
  auto model = GetAssetWorldTransform(transform);
  shader->SetMaterial(material);
  shader->SetMaterialFallback(mesh, litMaterial.fallback, materialVBO);
  shader->SetTransform(mesh, model, transformVBO);
  shader->Draw(mesh);
}
int RenderingSystem::CreateCubeMapFromEquirect(unsigned int equirectAssetId) {
  auto equirectTexture = app.GetAssetComponent<Texture>(equirectAssetId);
  auto width = equirectTexture->width / 4; // 360 / 90
  auto height = equirectTexture->height / 2; // 180 / 90
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
  auto shader = GetShader(MaterialType::EquirectCubeMap);
  shader->Use();
  shader->SetUniform("equirect", 0);
  shader->SetUniform("projection", projection);
  auto model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
  shader->SetUniform("model", model);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, equirectTexture->id);
  glViewport(0, 0, width, height);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  auto assetId = app.GetAssetId("CubeInverted"); // faces of an inverted cube mesh look towards the origin where the camera is (this is needed since backface culling is enabled)
  auto mesh = app.GetAssetComponent<Mesh>(assetId);
  if (!mesh) {
    spdlog::warn("Cube mesh not found. Cannot convert equirectangular map to cube map.");
    return -1;
  }
  auto isEXR = equirectTexture->type == TextureType::EXR;
  glm::mat4 viewMatrices[] = {glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(1.0f, .0f, .0f), glm::vec3(.0f, isEXR ? 1.0f : -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(-1.0f, .0f, .0f), glm::vec3(.0f, isEXR ? 1.0f : -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, isEXR ? -1.0f : 1.0f, .0f), glm::vec3(.0f, .0f, isEXR ? -1.0f : 1.0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, isEXR ? 1.0f : -1.0f, .0f), glm::vec3(.0f, .0f, isEXR ? 1.0f : -1.0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, .0f, isEXR ? -1.0f : 1.0f), glm::vec3(.0f, isEXR ? 1.0f : -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, .0f, isEXR ? 1.0f : -1.0f), glm::vec3(.0f, isEXR ? 1.0f : -1.0f, .0f))};
  for (auto i = 0; i < 6; ++i) {
    shader->SetUniform("view", viewMatrices[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubeMapTextureId, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shader->Draw(mesh);
  }
  auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    spdlog::error("Framebuffer not complete.");
    return -1;
  }
  std::string name = app.GetAssetName(equirectAssetId) + "CubeMap";
  auto cubeMapAssetId = app.CreateAsset(name);
  auto cubeMapTexture = app.AddAssetComponent<Texture>(cubeMapAssetId);
  cubeMapTexture->id = cubeMapTextureId;
  cubeMapTexture->type = TextureType::CubeMap;
  spdlog::info("HDR cube map is created: {}.", name);
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
