#define GLM_ENABLE_EXPERIMENTAL
#include <system/rendering.hpp>
#include <application.hpp>
#include <component/component.hpp>
#include <component/shader.hpp>
#include <component/transform.hpp>
#include <spdlog/spdlog.h>
#include <system/system.hpp>
namespace kuki {
RenderingSystem::RenderingSystem(Application& app)
  : System(app) {}
void RenderingSystem::Start() {
  glCreateBuffers(1, &materialVBO);
  glCreateBuffers(1, &transformVBO);
  glGenFramebuffers(1, &framebuffer);
  glGenFramebuffers(1, &framebufferMulti);
  glGenRenderbuffers(1, &renderbuffer);
  glGenRenderbuffers(1, &renderbufferMulti);
  auto brdfCompute = new ComputeShader("BRDF", "shader/brdf.comp", ComputeType::BRDF);
  computes.insert({brdfCompute->GetType(), brdfCompute});
  auto cubeMapEquirectShader = new Shader("CubeMapEquirect", "shader/standard_m.vert", "shader/cubemap_equirect.frag", MaterialType::CubeMapEquirect);
  auto cubeMapIrradianceShader = new Shader("CubeMapIrradiance", "shader/standard_mvp.vert", "shader/cubemap_irradiance.frag", MaterialType::CubeMapIrradiance);
  auto cubeMapPrefilterShader = new Shader("CubeMapPrefilter", "shader/standard_mvp.vert", "shader/cubemap_prefilter.frag", MaterialType::CubeMapPrefilter);
  auto equirectCubeMapShader = new Shader("EquirectCubeMap", "shader/standard_mvp.vert", "shader/equirect_cubemap.frag", MaterialType::EquirectCubeMap);
  auto litShader = new LitShader("Lit", "shader/lit.vert", "shader/lit.frag");
  auto postprocShader = new Shader("Postprocessing", "shader/standard_m.vert", "shader/postprocessing.frag", MaterialType::Postprocessing);
  auto skyboxShader = new Shader("Skybox", "shader/skybox.vert", "shader/skybox.frag", MaterialType::Skybox);
  auto unlitShader = new UnlitShader("Unlit", "shader/unlit.vert", "shader/unlit.frag");
  shaders.insert({cubeMapEquirectShader->GetType(), cubeMapEquirectShader});
  shaders.insert({cubeMapIrradianceShader->GetType(), cubeMapIrradianceShader});
  shaders.insert({cubeMapPrefilterShader->GetType(), cubeMapPrefilterShader});
  shaders.insert({equirectCubeMapShader->GetType(), equirectCubeMapShader});
  shaders.insert({litShader->GetType(), litShader});
  shaders.insert({postprocShader->GetType(), postprocShader});
  shaders.insert({skyboxShader->GetType(), skyboxShader});
  shaders.insert({unlitShader->GetType(), unlitShader});
  assetCam.Update();
}
void RenderingSystem::Update(float deltaTime) {
  static std::deque<float> times;
  static auto accumulatedTime = .0f;
  times.push_back(deltaTime);
  accumulatedTime += deltaTime;
  while (accumulatedTime > 1.0f && !times.empty()) {
    auto lastTime = times.front();
    accumulatedTime -= lastTime;
    times.pop_front();
  }
  fps = times.size();
  UpdateTransforms();
}
void RenderingSystem::Shutdown() {
  glDeleteFramebuffers(1, &framebuffer);
  glDeleteFramebuffers(1, &framebufferMulti);
  glDeleteRenderbuffers(1, &renderbuffer);
  glDeleteRenderbuffers(1, &renderbufferMulti);
  glDeleteVertexArrays(1, &materialVBO);
  glDeleteVertexArrays(1, &transformVBO);
  for (const auto& [_, compute] : computes)
    delete compute;
  for (const auto& [_, shader] : shaders)
    delete shader;
  computes.clear();
  shaders.clear();
  texturePool.Clear();
}
int RenderingSystem::GetFPS() const {
  return fps;
}
bool RenderingSystem::UpdateAttachments(unsigned int framebuffer, unsigned int renderbuffer, unsigned int texture, const TextureParams& params) {
  if (framebuffer == 0) {
    spdlog::error("Framebuffer is not initialized.");
    return false;
  }
  if (renderbuffer == 0) {
    spdlog::error("Renderbuffer attachment is not initialized.");
    return false;
  }
  if (texture == 0) {
    spdlog::error("Texture attachment is not initialized.");
    return false;
  }
  const auto multiSample = params.samples > 1;
  const auto cubeMap = params.target == GL_TEXTURE_CUBE_MAP;
  glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
  if (multiSample)
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, params.samples, GL_DEPTH24_STENCIL8, params.width, params.height);
  else
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, params.width, params.height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cubeMap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : params.target, texture, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
  auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    spdlog::error("Framebuffer is incomplete ({0:x}).", status);
    return false;
  }
  return true;
}
bool RenderingSystem::wireframeMode = false;
void RenderingSystem::ToggleWireframeMode() {
  wireframeMode = !wireframeMode;
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
  case MaterialType::CubeMapPrefilter:
    return shaders[MaterialType::CubeMapPrefilter];
  case MaterialType::Postprocessing:
    return shaders[MaterialType::Postprocessing];
  default:
    return nullptr;
  }
}
ComputeShader* RenderingSystem::GetCompute(ComputeType type) {
  switch (type) {
  case ComputeType::BRDF:
    return computes[ComputeType::BRDF];
  default:
    return nullptr;
  }
}
void RenderingSystem::UpdateTransforms() {
  auto updateNeeded = false;
  // propagate dirty flags
  app.ForEachRootEntity([this, &updateNeeded](unsigned int id) {
    auto transform = app.GetEntityComponent<Transform>(id);
    if (transform && transform->localDirty) {
      updateNeeded = true;
      UpdateChildFlags(id);
    }
  });
  if (!updateNeeded)
    return;
  // re-calculate transform matrices
  app.ForEachEntity<Transform>([this](unsigned int id, Transform* _) {
    app.UpdateEntityWorldTransform(id);
  });
}
void RenderingSystem::UpdateChildFlags(unsigned int id) {
  app.ForEachChildEntity(id, [this](unsigned int childId) {
    auto childTransform = app.GetEntityComponent<Transform>(childId);
    if (childTransform)
      childTransform->worldDirty = true;
    UpdateChildFlags(childId);
  });
}
} // namespace kuki
