#define GLM_ENABLE_EXPERIMENTAL
#include <application.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <cstdint>
#include <gl_constants.hpp>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <mesh.hpp>
#include <rendering_system.hpp>
#include <shader.hpp>
#include <spdlog/spdlog.h>
#include <texture_params.hpp>
#include <texture_pool.hpp>
#include <vector>
namespace kuki {
int RenderingSystem::RenderSceneToTexture(Camera* camera) {
  if (!camera)
    return -1;
  auto& config = app.GetConfig();
  const auto width = config.screenWidth;
  const auto height = config.screenHeight;
  // FIXME: the following prevents users from experimenting with different aspect ratios in the editor
  const auto aspect = static_cast<float>(width) / height;
  if (camera->aspectRatio != aspect) {
    camera->aspectRatio = aspect;
    camera->rotationDirty = true;
    camera->uboDirty = true;
  }
  const TextureParams singleParams{width, height, GL_TEXTURE_2D, GL_RGB16F, 1, 1};
  const TextureParams multiParams{width, height, GL_TEXTURE_2D_MULTISAMPLE, GL_RGB16F, 4, 1};
  auto framebufferMulti = framebufferPool.Request(multiParams);
  auto framebufferSingle = framebufferPool.Request(singleParams);
  auto renderbufferMulti = renderbufferPool.Request(multiParams);
  auto renderbufferSingle = renderbufferPool.Request(singleParams);
  auto textureMulti = texturePool.Request(multiParams);
  auto textureSingle = texturePool.Request(singleParams);
  UpdateAttachments(multiParams, framebufferMulti, renderbufferMulti, textureMulti);
  UpdateAttachments(singleParams, framebufferSingle, renderbufferSingle, textureSingle);
  glBindFramebuffer(GL_FRAMEBUFFER, framebufferMulti);
  glViewport(0, 0, width, height);
  glClearColor(.0f, .0f, .0f, .0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  auto sceneCamId = app.GetEntityId("SceneCamera");
  auto sceneCam = app.GetEntityComponent<Camera>(sceneCamId);
  if (!sceneCam)
    sceneCam = camera;
  // HACK: we may not get the scene camera by calling GetActiveCamera() because editor camera is also in the scene
  // TODO: hide editor camera from scene hierarchy or tag them to distinguish
  DrawScene(sceneCam, camera);
  DrawGizmos(sceneCam, camera);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferMulti);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebufferSingle);
  glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  auto texturePost = texturePool.Request(singleParams);
  ApplyPostProc(textureSingle, texturePost, singleParams);
  framebufferPool.Release(multiParams, framebufferMulti);
  framebufferPool.Release(singleParams, framebufferSingle);
  renderbufferPool.Release(multiParams, renderbufferMulti);
  renderbufferPool.Release(singleParams, renderbufferSingle);
  texturePool.Release(multiParams, textureMulti);
  texturePool.Release(singleParams, texturePost);
  texturePool.Release(singleParams, textureSingle);
  return texturePost;
}
void RenderingSystem::ApplyPostProc(GLConst::UINT textureIn, GLConst::UINT textureOut, const TextureParams& params) {
  auto bloomShader = GetShader(MaterialType::Bloom);
  if (!bloomShader) {
    spdlog::warn("Shader not found: {}.", EnumTraits<MaterialType>().GetNames().at(static_cast<uint8_t>(MaterialType::Bloom)));
    return;
  }
  auto brightShader = GetShader(MaterialType::Bright);
  if (!brightShader) {
    spdlog::warn("Shader not found: {}.", EnumTraits<MaterialType>().GetNames().at(static_cast<uint8_t>(MaterialType::Bright)));
    return;
  }
  auto blurShader = GetShader(MaterialType::Blur);
  if (!blurShader) {
    spdlog::warn("Shader not found: {}.", EnumTraits<MaterialType>().GetNames().at(static_cast<uint8_t>(MaterialType::Blur)));
    return;
  }
  auto frameId = app.GetAssetId("Frame");
  auto mesh = app.GetAssetComponent<Mesh>(frameId);
  if (!mesh) {
    spdlog::warn("Frame mesh not found.");
    return;
  }
  auto framebufferPing = framebufferPool.Request(params);
  auto framebufferPong = framebufferPool.Request(params);
  auto renderbufferPing = renderbufferPool.Request(params);
  auto renderbufferPong = renderbufferPool.Request(params);
  auto texturePing = texturePool.Request(params);
  auto texturePong = texturePool.Request(params);
  auto textureNormal = texturePool.Request(params);
  UpdateAttachments(params, framebufferPong, renderbufferPong, textureNormal, texturePong);
  glBindFramebuffer(GL_FRAMEBUFFER, framebufferPong);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textureIn);
  brightShader->Use();
  brightShader->SetUniform("image", 0);
  glViewport(0, 0, params.width, params.height);
  glClearColor(.0f, .0f, .0f, .0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  brightShader->SetUniform("model", glm::mat4(1.0f));
  GLConst::UINT attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
  glDrawBuffers(2, attachments);
  brightShader->Draw(mesh);
  const auto blurPasses = 8;
  blurShader->Use();
  blurShader->SetUniform("model", glm::mat4(1.0f));
  blurShader->SetUniform("image", 0);
  for (auto i = 0; i < blurPasses; ++i) {
    auto pingPong = i % 2 == 0;
    auto framebuffer = pingPong ? framebufferPing : framebufferPong;
    auto renderbuffer = pingPong ? renderbufferPing : renderbufferPong;
    auto textureTarget = pingPong ? texturePing : texturePong;
    auto textureSource = pingPong ? texturePong : texturePing;
    UpdateAttachments(params, framebuffer, renderbuffer, textureTarget);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glBindTexture(GL_TEXTURE_2D, textureSource);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    blurShader->SetUniform("horizontal", pingPong);
    blurShader->Draw(mesh);
  }
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  UpdateAttachments(params, framebufferPing, renderbufferPing, textureOut);
  glBindFramebuffer(GL_FRAMEBUFFER, framebufferPing);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textureNormal);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, texturePong);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  bloomShader->Use();
  bloomShader->SetUniform("hdrImage", 0);
  bloomShader->SetUniform("bloomImage", 1);
  bloomShader->SetUniform("exposure", 1.0f);
  bloomShader->SetUniform("gamma", 2.2f);
  bloomShader->SetUniform("model", glm::mat4(1.0f));
  bloomShader->Draw(mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  framebufferPool.Release(params, framebufferPing);
  framebufferPool.Release(params, framebufferPong);
  renderbufferPool.Release(params, renderbufferPing);
  renderbufferPool.Release(params, renderbufferPong);
  texturePool.Release(params, texturePing);
  texturePool.Release(params, texturePong);
  texturePool.Release(params, textureNormal);
}
} // namespace kuki
