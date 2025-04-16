#define GLM_ENABLE_EXPERIMENTAL
#include <application.hpp>
#include <component/material.hpp>
#include <component/shader.hpp>
#include <glad/glad.h>
#include <render_system.hpp>
#include <spdlog/spdlog.h>
#include <system.hpp>
#include <variant>
namespace kuki {
RenderSystem::RenderSystem(Application& app)
  : System(app), texturePool(CreatePooledTexture, DeletePooledTexture, 16) {}
RenderSystem::~RenderSystem() {
  for (const auto& [_, shader] : shaders)
    delete shader;
  shaders.clear();
}
void RenderSystem::Start() {
  assetCam.Update();
  auto litShader = new Shader("Lit", "shader/lit.vert", "shader/lit.frag");
  auto unlitShader = new Shader("Unlit", "shader/unlit.vert", "shader/unlit.frag");
  auto skyboxShader = new Shader("Skybox", "shader/skybox.vert", "shader/skybox.frag");
  auto skyboxFlatShader = new Shader("SkyboxFlat", "shader/skybox_flat.vert", "shader/skybox_flat.frag");
  shaders.insert({litShader->GetName(), litShader});
  shaders.insert({unlitShader->GetName(), unlitShader});
  shaders.insert({skyboxShader->GetName(), skyboxShader});
  shaders.insert({skyboxFlatShader->GetName(), skyboxFlatShader});
  glCreateBuffers(1, &instanceVBO);
  glCreateBuffers(1, &materialVBO);
  glGenTextures(1, &sceneTexture);
  glGenTextures(1, &sceneMultiTexture);
}
void RenderSystem::Shutdown() {
  glDeleteFramebuffers(1, &assetFBO);
  glDeleteFramebuffers(1, &sceneFBO);
  glDeleteFramebuffers(1, &sceneMultiFBO);
  glDeleteRenderbuffers(1, &assetRBO);
  glDeleteRenderbuffers(1, &sceneMultiRBO);
  glDeleteRenderbuffers(1, &sceneRBO);
  glDeleteTextures(1, &sceneMultiTexture);
  glDeleteTextures(1, &sceneTexture);
  glDeleteVertexArrays(1, &instanceVBO);
  glDeleteVertexArrays(1, &materialVBO);
}
bool RenderSystem::UpdateBuffers(unsigned int& framebuffer, unsigned int& renderbuffer, unsigned int& texture, int width, int height, int samples) {
  auto multi = samples > 1;
  auto texTarget = multi ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
  glBindTexture(texTarget, texture);
  if (multi)
    glTexImage2DMultisample(texTarget, samples, GL_RGBA16F, width, height, GL_TRUE);
  else {
    glTexImage2D(texTarget, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(texTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(texTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }
  glBindTexture(texTarget, 0);
  if (renderbuffer == 0)
    glGenRenderbuffers(1, &renderbuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
  if (multi)
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width, height);
  else
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  if (framebuffer == 0)
    glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texTarget, texture, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
  auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    spdlog::error("Framebuffer is incomplete ({0:x}).", status);
    return false;
  }
  return true;
}
void RenderSystem::ToggleWireframeMode() {
  static auto enabled = false;
  enabled = !enabled;
  if (enabled)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  else
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
Shader* RenderSystem::GetMaterialShader(const Material& material) {
  if (std::holds_alternative<UnlitMaterial>(material.material))
    return shaders["Unlit"];
  return shaders["Lit"];
}
unsigned int RenderSystem::CreatePooledTexture() {
  unsigned int id;
  glGenTextures(1, &id);
  return id;
}
void RenderSystem::DeletePooledTexture(unsigned int id) {
  glDeleteTextures(1, &id);
}
} // namespace kuki
