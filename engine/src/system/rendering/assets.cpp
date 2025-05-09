#define GLM_ENABLE_EXPERIMENTAL
#include <glad/glad.h>
#include <application.hpp>
#include <component/component.hpp>
#include <component/light.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/shader.hpp>
#include <component/texture.hpp>
#include <component/skybox.hpp>
#include <component/transform.hpp>
#include <functional>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtx/quaternion.hpp>
#include <spdlog/spdlog.h>
#include <system/rendering.hpp>
#include <vector>
#include <cstdint>
namespace kuki {
int RenderingSystem::RenderAssetToTexture(unsigned int assetId, int size) {
  auto [texture, skybox] = app.GetAssetComponents<Texture, Skybox>(assetId);
  auto isCubeMap = false;
  auto isPresent = assetToTexture.find(assetId) != assetToTexture.end();
  if (!isPresent) {
    if (texture) {
      isCubeMap = texture->type == TextureType::CubeMap;
      if (!isCubeMap) // NOTE: for textures that are not cubemaps, we can directly use the OpenGL texture IDs
        assetToTexture[assetId] = texture->id;
    } else {
      auto itemId = texturePool.Request();
      assetToTexture[assetId] = *texturePool.Get(itemId);
    }
  }
  auto textureId = assetToTexture[assetId]; // NOTE: this is the target texture that the asset will be rendered to
  if (!isPresent && !texture) {
    size = std::max(1, size);
    UpdateAttachments(framebuffer, renderbufferAsset, textureId, size, size, 1, isCubeMap);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, size, size);
    glClearColor(.0f, .0f, .0f, .0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    if (isCubeMap || skybox)
      DrawSkyboxAsset(assetId);
    else
      DrawAsset(assetId);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  if (skybox)
    skybox->id.z = textureId;
  return textureId;
}
void RenderingSystem::DrawSkyboxAsset(unsigned int id) {
  static UnlitMaterial material;
  auto [texture, skybox] = app.GetAssetComponents<Texture, Skybox>(id);
  if (!texture && !skybox)
    return;
  material.data.base = skybox ? skybox->id.x : texture->id;
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
Texture RenderingSystem::CreateCubeMapFromEquirect(Texture equirect) {
  Texture texture{};
  texture.type = TextureType::CubeMap;
  auto shader = GetShader(MaterialType::EquirectCubeMap);
  if (!shader) {
    spdlog::warn("Shader not found: {}.", EnumTraits<MaterialType>().GetNames().at(static_cast<uint8_t>(MaterialType::EquirectCubeMap)));
    return texture;
  }
  auto cubeId = app.GetAssetId("CubeInverted");
  auto mesh = app.GetAssetComponent<Mesh>(cubeId);
  if (!mesh) {
    spdlog::warn("Cube mesh not found.");
    return texture;
  }
  auto width = equirect.width / 4; // 360 / 90
  auto height = equirect.height / 2; // 180 / 90
  auto projection = glm::perspective(glm::radians(90.0f), 1.0f, .1f, 10.0f);
  shader->Use();
  shader->SetUniform("equirect", 0);
  shader->SetUniform("projection", projection);
  auto model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
  shader->SetUniform("model", model);
  auto isEXR = equirect.type == TextureType::EXR;
  glm::mat4 viewMatrices[] = {glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(1.0f, .0f, .0f), glm::vec3(.0f, isEXR ? 1.0f : -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(-1.0f, .0f, .0f), glm::vec3(.0f, isEXR ? 1.0f : -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, isEXR ? -1.0f : 1.0f, .0f), glm::vec3(.0f, .0f, isEXR ? -1.0f : 1.0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, isEXR ? 1.0f : -1.0f, .0f), glm::vec3(.0f, .0f, isEXR ? 1.0f : -1.0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, .0f, isEXR ? -1.0f : 1.0f), glm::vec3(.0f, isEXR ? 1.0f : -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, .0f, isEXR ? 1.0f : -1.0f), glm::vec3(.0f, isEXR ? 1.0f : -1.0f, .0f))};
  unsigned int cubeMapId{};
  UpdateAttachments(framebuffer, renderbufferAsset, cubeMapId, width, height, 1, true);
  texture.id = cubeMapId;
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, equirect.id);
  glViewport(0, 0, width, height);
  for (auto i = 0; i < 6; ++i) {
    shader->SetUniform("view", viewMatrices[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubeMapId, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shader->Draw(mesh);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return texture;
}
Texture RenderingSystem::CreateIrradianceMapFromCubeMap(Texture cubeMap) {
  Texture texture{};
  texture.type = TextureType::CubeMap;
  auto shader = GetShader(MaterialType::CubeMapIrradiance);
  if (!shader) {
    spdlog::warn("Shader not found: {}.", EnumTraits<MaterialType>().GetNames().at(static_cast<uint8_t>(MaterialType::CubeMapIrradiance)));
    return texture;
  }
  auto cubeId = app.GetAssetId("CubeInverted");
  auto mesh = app.GetAssetComponent<Mesh>(cubeId);
  if (!mesh) {
    spdlog::warn("Cube mesh not found.");
    return texture;
  }
  auto width = 32;
  auto height = 32;
  texture.width = width;
  texture.height = height;
  auto projection = glm::perspective(glm::radians(90.0f), 1.0f, .1f, 10.0f);
  shader->Use();
  shader->SetUniform("cubeMap", 0);
  shader->SetUniform("projection", projection);
  auto model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
  shader->SetUniform("model", model);
  glm::mat4 viewMatrices[] = {glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(1.0f, .0f, .0f), glm::vec3(.0f, -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(-1.0f, .0f, .0f), glm::vec3(.0f, -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, 1.0f, .0f), glm::vec3(.0f, .0f, 1.0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, -1.0f, .0f), glm::vec3(.0f, .0f, -1.0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, .0f, 1.0f), glm::vec3(.0f, -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, .0f, -1.0f), glm::vec3(.0f, -1.0f, .0f))};
  unsigned int irradianceMapId{};
  UpdateAttachments(framebuffer, renderbufferAsset, irradianceMapId, width, height, 1, true);
  texture.id = irradianceMapId;
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap.id);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glViewport(0, 0, width, height);
  for (auto i = 0; i < 6; ++i) {
    shader->SetUniform("view", viewMatrices[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMapId, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shader->Draw(mesh);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return texture;
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
