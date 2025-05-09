#define GLM_ENABLE_EXPERIMENTAL
#include <glad/glad.h>
#include <application.hpp>
#include <component/component.hpp>
#include <component/shader.hpp>
#include <spdlog/spdlog.h>
#include <system/rendering.hpp>
#include <system/system.hpp>
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
  auto skyboxShader = new Shader("Skybox", "shader/skybox.vert", "shader/skybox.frag", MaterialType::Skybox);
  auto cubeMapEquirectShader = new Shader("CubeMapEquirect", "shader/cubemap_equirect.vert", "shader/cubemap_equirect.frag", MaterialType::CubeMapEquirect);
  auto equirectCubeMapShader = new Shader("EquirectCubeMap", "shader/equirect_cubemap.vert", "shader/equirect_cubemap.frag", MaterialType::EquirectCubeMap);
  auto cubeMapIrradianceShader = new Shader("CubeMapIrradiance", "shader/cubemap_irradiance.vert", "shader/cubemap_irradiance.frag", MaterialType::CubeMapIrradiance);
  shaders.insert({litShader->GetType(), litShader});
  shaders.insert({unlitShader->GetType(), unlitShader});
  shaders.insert({skyboxShader->GetType(), skyboxShader});
  shaders.insert({cubeMapEquirectShader->GetType(), cubeMapEquirectShader});
  shaders.insert({equirectCubeMapShader->GetType(), equirectCubeMapShader});
  shaders.insert({cubeMapIrradianceShader->GetType(), cubeMapIrradianceShader});
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
bool RenderingSystem::UpdateAttachments(unsigned int& framebuffer, unsigned int& renderbuffer, unsigned int& texture, int width, int height, int samples, bool cubeMap) {
  auto multi = samples > 1;
  auto texTarget = cubeMap ? GL_TEXTURE_CUBE_MAP : multi ? GL_TEXTURE_2D_MULTISAMPLE
                                                         : GL_TEXTURE_2D;
  if (texture == 0)
    glGenTextures(1, &texture);
  glBindTexture(texTarget, texture);
  if (cubeMap) {
    for (auto i = 0; i < 6; ++i)
      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  } else if (multi)
    glTexImage2DMultisample(texTarget, samples, GL_RGBA16F, width, height, GL_TRUE);
  else {
    glTexImage2D(texTarget, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(texTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  }
  glBindTexture(texTarget, 0);
  if (renderbuffer == 0)
    glGenRenderbuffers(1, &renderbuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
  if (!cubeMap && multi)
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width, height);
  else
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  if (framebuffer == 0)
    glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  if (!cubeMap)
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
Shader* RenderingSystem::GetShader(MaterialType type) {
  switch (type) {
  case MaterialType::Lit:
    return shaders[MaterialType::Lit];
  case MaterialType::Unlit:
    return shaders[MaterialType::Unlit];
  case MaterialType::Skybox:
    return shaders[MaterialType::Skybox];
  case MaterialType::CubeMapEquirect:
    return shaders[MaterialType::CubeMapEquirect];
  case MaterialType::EquirectCubeMap:
    return shaders[MaterialType::EquirectCubeMap];
  case MaterialType::CubeMapIrradiance:
    return shaders[MaterialType::CubeMapIrradiance];
  default:
    return nullptr;
  }
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
