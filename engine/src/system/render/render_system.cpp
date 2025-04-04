#define GLM_ENABLE_EXPERIMENTAL
#include <application.hpp>
#include <render_system.hpp>
#include <iostream>
#include <ios>
#include <variant>
#include <component/material.hpp>
#include <component/shader.hpp>
#include <system.hpp>
#include <glad/glad.h>
RenderSystem::RenderSystem(Application& app)
  : System(app), texturePool(CreateTexture, DeleteTexture, 16) {}
RenderSystem::~RenderSystem() {
  for (const auto& [_, shader] : shaders)
    delete shader;
  shaders.clear();
}
void RenderSystem::Start() {
  assetCam.Update();
  auto phongShader = new Shader("Phong", "shader/phong.vert", "shader/phong.frag");
  auto blinnPhongShader = new Shader("BlinnPhong", "shader/blinn_phong.vert", "shader/blinn_phong.frag");
  auto pbrShader = new Shader("PBR", "shader/pbr.vert", "shader/pbr.frag");
  auto unlitShader = new Shader("Unlit", "shader/unlit.vert", "shader/unlit.frag");
  auto unlit2Shader = new Shader("Unlit2", "shader/unlit2.vert", "shader/unlit2.frag");
  auto skyboxShader = new Shader("Skybox", "shader/skybox.vert", "shader/skybox.frag");
  shaders.insert({phongShader->GetName(), phongShader});
  shaders.insert({blinnPhongShader->GetName(), blinnPhongShader});
  shaders.insert({pbrShader->GetName(), pbrShader});
  shaders.insert({unlitShader->GetName(), unlitShader});
  shaders.insert({unlit2Shader->GetName(), unlit2Shader});
  shaders.insert({skyboxShader->GetName(), skyboxShader});
  glGenBuffers(1, &instanceVBO);
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
}
bool RenderSystem::UpdateBuffers(unsigned int& fbo, unsigned int& rbo, unsigned int& texture, int width, int height, int samples) {
  auto multi = samples > 1;
  if (texture == 0)
    glCreateTextures(multi ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, 1, &texture);
  if (multi) {
    glTextureStorage2DMultisample(texture, samples, GL_RGBA8, width, height, GL_TRUE);
  } else {
    glTextureStorage2D(texture, 1, GL_RGBA8, width, height);
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }
  if (rbo == 0)
    glCreateRenderbuffers(1, &rbo);
  if (multi)
    glNamedRenderbufferStorageMultisample(rbo, samples, GL_DEPTH24_STENCIL8, width, height);
  else
    glNamedRenderbufferStorage(rbo, GL_DEPTH24_STENCIL8, width, height);
  if (fbo == 0)
    glCreateFramebuffers(1, &fbo);
  glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, texture, 0);
  glNamedFramebufferRenderbuffer(fbo, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
  auto status = glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "Framebuffer is incomplete. Status: 0x" << std::hex << status << std::dec << std::endl;
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
  if (std::holds_alternative<PhongMaterial>(material.material))
    return shaders["BlinnPhong"];
  else if (std::holds_alternative<UnlitMaterial>(material.material))
    return shaders["Unlit"];
  else
    return shaders["PBR"];
}
unsigned int RenderSystem::CreateTexture() {
  unsigned int id;
  glGenTextures(1, &id);
  return id;
}
void RenderSystem::DeleteTexture(unsigned int id) {
  glDeleteTextures(1, &id);
}
