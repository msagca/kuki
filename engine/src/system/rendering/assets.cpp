#define GLM_ENABLE_EXPERIMENTAL
#include <system/rendering.hpp>
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
#include <vector>
#include <cstdint>
#include <utility/pool.hpp>
#include <cmath>
namespace kuki {
int RenderingSystem::RenderAssetToTexture(unsigned int assetId, int size) {
  static const auto MIN_SIZE = 16;
  auto textureSize = std::max(MIN_SIZE, size);
  auto [texture, skybox] = app.GetAssetComponents<Texture, Skybox>(assetId);
  auto isCubeMap = false;
  auto isPresent = assetToTexture.find(assetId) != assetToTexture.end();
  TextureParams params{textureSize, textureSize, GL_TEXTURE_2D, GL_RGBA16F};
  if (!isPresent) {
    if (texture) {
      isCubeMap = texture->type == TextureType::CubeMap;
      if (!isCubeMap) // NOTE: for textures that are not cubemaps, we can directly use the OpenGL texture IDs
        assetToTexture[assetId] = texture->id;
    } else {
      auto textureId = texturePool.Request(params);
      assetToTexture[assetId] = textureId;
    }
  }
  auto textureIdPost = assetToTexture[assetId]; // NOTE: this is the target texture that the asset will be rendered to
  if (!isPresent && !texture) {
    params.target = isCubeMap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
    auto textureIdPre = texturePool.Request(params);
    UpdateAttachments(framebuffer, renderbuffer, textureIdPre, params);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, textureSize, textureSize);
    glClearColor(.0f, .0f, .0f, .0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    if (isCubeMap || skybox)
      DrawSkyboxAsset(assetId);
    else
      DrawAsset(assetId);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ApplyPostProc(textureIdPre, textureIdPost, params);
    texturePool.Release(params, textureIdPre);
  }
  if (skybox)
    skybox->data.preview = textureIdPost;
  return textureIdPost;
}
void RenderingSystem::DrawSkyboxAsset(unsigned int id) {
  auto skybox = app.GetAssetComponent<Skybox>(id);
  if (!skybox || skybox->data.skybox == 0)
    return;
  auto frameId = app.GetAssetId("Frame");
  auto mesh = app.GetAssetComponent<Mesh>(frameId);
  if (!mesh)
    return;
  static UnlitMaterial material;
  material.data.base = skybox->data.skybox;
  material.type = MaterialType::CubeMapEquirect;
  auto shader = GetShader(MaterialType::CubeMapEquirect);
  shader->Use();
  shader->SetUniform("model", glm::mat4(1.0));
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
  const auto width = equirect.width / 4; // 360 / 90
  const auto height = equirect.height / 2; // 180 / 90
  auto projection = glm::perspective(glm::radians(90.0f), 1.0f, .1f, 10.0f);
  shader->Use();
  shader->SetUniform("projection", projection);
  auto model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
  shader->SetUniform("model", model);
  auto isEXR = equirect.type == TextureType::EXR;
  glm::mat4 viewMatrices[] = {glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(1.0f, .0f, .0f), glm::vec3(.0f, isEXR ? 1.0f : -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(-1.0f, .0f, .0f), glm::vec3(.0f, isEXR ? 1.0f : -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, isEXR ? -1.0f : 1.0f, .0f), glm::vec3(.0f, .0f, isEXR ? -1.0f : 1.0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, isEXR ? 1.0f : -1.0f, .0f), glm::vec3(.0f, .0f, isEXR ? 1.0f : -1.0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, .0f, isEXR ? -1.0f : 1.0f), glm::vec3(.0f, isEXR ? 1.0f : -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, .0f, isEXR ? 1.0f : -1.0f), glm::vec3(.0f, isEXR ? 1.0f : -1.0f, .0f))};
  TextureParams params{width, height, GL_TEXTURE_CUBE_MAP};
  auto textureId = texturePool.Request(params);
  UpdateAttachments(framebuffer, renderbuffer, textureId, params);
  texture.id = textureId;
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glActiveTexture(GL_TEXTURE0);
  shader->SetUniform("equirect", 0);
  glBindTexture(GL_TEXTURE_2D, equirect.id);
  glViewport(0, 0, width, height);
  for (auto i = 0; i < 6; ++i) {
    shader->SetUniform("view", viewMatrices[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, textureId, 0);
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
  const auto width = 32;
  const auto height = width;
  texture.width = width;
  texture.height = height;
  auto projection = glm::perspective(glm::radians(90.0f), 1.0f, .1f, 10.0f);
  shader->Use();
  shader->SetUniform("projection", projection);
  auto model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
  shader->SetUniform("model", model);
  glm::mat4 viewMatrices[] = {glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(1.0f, .0f, .0f), glm::vec3(.0f, -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(-1.0f, .0f, .0f), glm::vec3(.0f, -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, 1.0f, .0f), glm::vec3(.0f, .0f, 1.0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, -1.0f, .0f), glm::vec3(.0f, .0f, -1.0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, .0f, 1.0f), glm::vec3(.0f, -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, .0f, -1.0f), glm::vec3(.0f, -1.0f, .0f))};
  TextureParams params{width, height, GL_TEXTURE_CUBE_MAP};
  auto textureId = texturePool.Request(params);
  UpdateAttachments(framebuffer, renderbuffer, textureId, params);
  texture.id = textureId;
  glActiveTexture(GL_TEXTURE0);
  shader->SetUniform("cubeMap", 0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap.id);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glViewport(0, 0, width, height);
  for (auto i = 0; i < 6; ++i) {
    shader->SetUniform("view", viewMatrices[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, textureId, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shader->Draw(mesh);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return texture;
}
Texture RenderingSystem::CreatePrefilterMapFromCubeMap(Texture cubeMap) {
  Texture texture{};
  texture.type = TextureType::CubeMap;
  auto shader = GetShader(MaterialType::CubeMapPrefilter);
  if (!shader) {
    spdlog::warn("Shader not found: {}.", EnumTraits<MaterialType>().GetNames().at(static_cast<uint8_t>(MaterialType::CubeMapPrefilter)));
    return texture;
  }
  auto cubeId = app.GetAssetId("CubeInverted");
  auto mesh = app.GetAssetComponent<Mesh>(cubeId);
  if (!mesh) {
    spdlog::warn("Cube mesh not found.");
    return texture;
  }
  const auto width = 1024;
  const auto height = width;
  texture.width = width;
  texture.height = height;
  auto projection = glm::perspective(glm::radians(90.0f), 1.0f, .1f, 10.0f);
  shader->Use();
  shader->SetUniform("projection", projection);
  auto model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
  shader->SetUniform("model", model);
  glm::mat4 viewMatrices[] = {glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(1.0f, .0f, .0f), glm::vec3(.0f, -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(-1.0f, .0f, .0f), glm::vec3(.0f, -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, 1.0f, .0f), glm::vec3(.0f, .0f, 1.0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, -1.0f, .0f), glm::vec3(.0f, .0f, -1.0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, .0f, 1.0f), glm::vec3(.0f, -1.0f, .0f)), glm::lookAt(glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, .0f, -1.0f), glm::vec3(.0f, -1.0f, .0f))};
  TextureParams params{width, height, GL_TEXTURE_CUBE_MAP};
  params.mipmaps = std::log2(width);
  auto textureId = texturePool.Request(params);
  UpdateAttachments(framebuffer, renderbuffer, textureId, params);
  texture.id = textureId;
  glActiveTexture(GL_TEXTURE0);
  shader->SetUniform("cubeMap", 0);
  shader->SetUniform("mipLevels", params.mipmaps);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap.id);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  for (auto i = 0; i < params.mipmaps; ++i) {
    auto mipWidth = static_cast<int>(width * std::pow(.5f, i));
    auto mipHeight = static_cast<int>(height * std::pow(.5f, i));
    glViewport(0, 0, mipWidth, mipHeight);
    auto roughness = static_cast<float>(i) / (params.mipmaps - 1);
    shader->SetUniform("roughness", roughness);
    shader->SetUniform("mipWidth", mipWidth);
    for (auto j = 0; j < 6; ++j) {
      shader->SetUniform("view", viewMatrices[j]);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, textureId, i);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      shader->Draw(mesh);
    }
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return texture;
}
Texture RenderingSystem::CreateBRDF_LUT() {
  Texture texture{};
  texture.type = TextureType::BRDF;
  auto shader = GetCompute(ComputeType::BRDF);
  if (!shader) {
    spdlog::warn("Compute shader not found: {}.", EnumTraits<ComputeType>().GetNames().at(static_cast<uint8_t>(ComputeType::BRDF)));
    return texture;
  }
  const auto width = 512;
  const auto height = width;
  texture.width = width;
  texture.height = height;
  TextureParams params{width, height, GL_TEXTURE_2D, GL_RG16F};
  auto textureId = texturePool.Request(params);
  texture.id = textureId;
  glBindImageTexture(0, textureId, 0, GL_FALSE, 0, GL_WRITE_ONLY, params.format);
  shader->Use();
  const auto workGroupSizeX = 16;
  const auto workGroupSizeY = 16;
  auto numGroupsX = (width + workGroupSizeX - 1) / workGroupSizeX;
  auto numGroupsY = (height + workGroupSizeY - 1) / workGroupSizeY;
  glDispatchCompute(numGroupsX, numGroupsY, 1);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
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
