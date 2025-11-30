#include <application.hpp>
#include <cmath>
#include <component.hpp>
#include <cstdint>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <mesh.hpp>
#include <pool.hpp>
#include <rendering_system.hpp>
#include <shader.hpp>
#include <spdlog/spdlog.h>
#include <texture.hpp>
#include <texture_params.hpp>
#include <transform.hpp>
#include <vector>
namespace kuki {
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
  auto framebuffer = framebufferPool.Request(params);
  auto renderbuffer = renderbufferPool.Request(params);
  auto textureId = texturePool.Request(params);
  UpdateAttachments(params, framebuffer, renderbuffer, textureId);
  texture.id = textureId;
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glActiveTexture(GL_TEXTURE0);
  shader->SetUniform("equirect", 0);
  glBindTexture(GL_TEXTURE_2D, equirect.id);
  glViewport(0, 0, width, height);
  for (auto i = 0; i < 6; ++i) {
    shader->SetUniform("view", viewMatrices[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, texture.id, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shader->Draw(mesh);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  framebufferPool.Release(params, framebuffer);
  renderbufferPool.Release(params, renderbuffer);
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
  const auto width = 64; // TODO: make this configurable
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
  auto framebuffer = framebufferPool.Request(params);
  auto renderbuffer = renderbufferPool.Request(params);
  auto textureId = texturePool.Request(params);
  UpdateAttachments(params, framebuffer, renderbuffer, textureId);
  texture.id = textureId;
  glActiveTexture(GL_TEXTURE0);
  shader->SetUniform("cubeMap", 0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap.id);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glViewport(0, 0, width, height);
  for (auto i = 0; i < 6; ++i) {
    shader->SetUniform("view", viewMatrices[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, texture.id, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shader->Draw(mesh);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  framebufferPool.Release(params, framebuffer);
  renderbufferPool.Release(params, renderbuffer);
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
  const auto width = 256; // TODO: make this configurable
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
  auto framebuffer = framebufferPool.Request(params);
  auto renderbuffer = renderbufferPool.Request(params);
  params.mipmaps = std::log2(width) + 1;
  auto textureId = texturePool.Request(params);
  UpdateAttachments(params, framebuffer, renderbuffer, textureId);
  texture.id = textureId;
  glActiveTexture(GL_TEXTURE0);
  shader->SetUniform("cubeMap", 0);
  shader->SetUniform("mipLevels", params.mipmaps);
  shader->SetUniform("mipWidth", width);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap.id);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  for (auto level = 0; level < params.mipmaps; ++level) {
    auto mipWidth = static_cast<int>(width * std::pow(.5f, level));
    auto mipHeight = static_cast<int>(height * std::pow(.5f, level));
    glViewport(0, 0, mipWidth, mipHeight);
    auto roughness = static_cast<float>(level) / (params.mipmaps - 1);
    shader->SetUniform("roughness", roughness);
    for (auto face = 0; face < 6; ++face) {
      shader->SetUniform("view", viewMatrices[face]);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, texture.id, level);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      shader->Draw(mesh);
    }
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  framebufferPool.Release(params, framebuffer);
  renderbufferPool.Release(params, renderbuffer);
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
  const auto width = 1024; // TODO: make this configurable
  const auto height = width;
  texture.width = width;
  texture.height = height;
  TextureParams params{width, height, GL_TEXTURE_2D, GL_RG16F};
  auto textureId = texturePool.Request(params);
  texture.id = textureId;
  glBindImageTexture(0, texture.id, 0, GL_FALSE, 0, GL_WRITE_ONLY, params.format);
  shader->Use();
  const auto workGroupSizeX = 16;
  const auto workGroupSizeY = 16;
  auto numGroupsX = (width + workGroupSizeX - 1) / workGroupSizeX;
  auto numGroupsY = (height + workGroupSizeY - 1) / workGroupSizeY;
  glDispatchCompute(numGroupsX, numGroupsY, 1);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  return texture;
}
} // namespace kuki
