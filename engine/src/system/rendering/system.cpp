#define GLM_ENABLE_EXPERIMENTAL
#include <glad/glad.h>
#include <application.hpp>
#include <component/material.hpp>
#include <component/shader.hpp>
#include <spdlog/spdlog.h>
#include <system/rendering.hpp>
#include <system/system.hpp>
#include <variant>
namespace kuki {
RenderingSystem::RenderingSystem(Application& app)
  : System(app), texturePool(CreatePooledTexture, DeletePooledTexture, 16) {}
RenderingSystem::~RenderingSystem() {
  for (const auto& [_, shader] : shaders)
    delete shader;
  shaders.clear();
}
void RenderingSystem::Start() {
  assetCam.Update();
  auto litShader = new LitShader("Lit", "shader/lit.vert", "shader/lit.frag");
  auto unlitShader = new UnlitShader("Unlit", "shader/unlit.vert", "shader/unlit.frag");
  auto skyboxShader = new Shader("Skybox", "shader/skybox.vert", "shader/skybox.frag");
  auto skyboxFlatShader = new Shader("SkyboxFlat", "shader/skybox_flat.vert", "shader/skybox_flat.frag");
  auto radianceShader = new Shader("Radiance", "shader/radiance.vert", "shader/radiance.frag");
  shaders.insert({litShader->GetName(), litShader});
  shaders.insert({unlitShader->GetName(), unlitShader});
  shaders.insert({skyboxShader->GetName(), skyboxShader});
  shaders.insert({skyboxFlatShader->GetName(), skyboxFlatShader});
  shaders.insert({radianceShader->GetName(), radianceShader});
  glCreateBuffers(1, &transformVBO);
  glCreateBuffers(1, &materialVBO);
  glGenTextures(1, &textureScene);
  glGenTextures(1, &textureSceneMulti);
}
void RenderingSystem::Shutdown() {
  glDeleteFramebuffers(1, &framebuffer);
  glDeleteFramebuffers(1, &framebufferMulti);
  glDeleteRenderbuffers(1, &renderbufferAsset);
  glDeleteRenderbuffers(1, &renderbufferSceneMulti);
  glDeleteRenderbuffers(1, &renderbufferScene);
  glDeleteTextures(1, &textureSceneMulti);
  glDeleteTextures(1, &textureScene);
  glDeleteVertexArrays(1, &transformVBO);
  glDeleteVertexArrays(1, &materialVBO);
}
bool RenderingSystem::UpdateAttachments(unsigned int& framebuffer, unsigned int& renderbuffer, unsigned int& texture, int width, int height, int samples) {
  auto multi = samples > 1;
  auto texTarget = multi ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
  if (texture == 0)
    glGenTextures(1, &texture);
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
void RenderingSystem::ToggleWireframeMode() {
  static auto enabled = false;
  enabled = !enabled;
  if (enabled)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  else
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
Shader* RenderingSystem::GetMaterialShader(const Material& material) {
  if (std::holds_alternative<UnlitMaterial>(material.material))
    return shaders["Unlit"];
  return shaders["Lit"];
}
unsigned int RenderingSystem::CreatePooledTexture() {
  unsigned int id;
  glGenTextures(1, &id);
  return id;
}
void RenderingSystem::DeletePooledTexture(unsigned int id) {
  glDeleteTextures(1, &id);
}
} // namespace kuki
