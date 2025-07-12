#define GLM_ENABLE_EXPERIMENTAL
#include <system/rendering.hpp>
#include <application.hpp>
#include <component/component.hpp>
#include <component/shader.hpp>
#include <component/transform.hpp>
#include <spdlog/spdlog.h>
#include <system/system.hpp>
#include <deque>
namespace kuki {
RenderingSystem::RenderingSystem(Application& app)
  : System(app) {}
RenderingSystem::~RenderingSystem() {
  Shutdown();
}
void RenderingSystem::Start() {
  glCreateBuffers(1, &materialVBO);
  glCreateBuffers(1, &transformVBO);
  auto bloomShader = new Shader("Postprocessing", "shader/standard_m.vert", "shader/bloom.frag", MaterialType::Bloom);
  auto blurShader = new Shader("Blur", "shader/standard_m.vert", "shader/blur.frag", MaterialType::Blur);
  auto brdfCompute = new ComputeShader("BRDF", "shader/brdf.comp", ComputeType::BRDF);
  auto brightShader = new Shader("Blur", "shader/standard_m.vert", "shader/bright.frag", MaterialType::Bright);
  auto cubeMapEquirectShader = new Shader("CubeMapEquirect", "shader/standard_m.vert", "shader/cubemap_equirect.frag", MaterialType::CubeMapEquirect);
  auto cubeMapIrradianceShader = new Shader("CubeMapIrradiance", "shader/standard_mvp.vert", "shader/cubemap_irradiance.frag", MaterialType::CubeMapIrradiance);
  auto cubeMapPrefilterShader = new Shader("CubeMapPrefilter", "shader/standard_mvp.vert", "shader/cubemap_prefilter.frag", MaterialType::CubeMapPrefilter);
  auto equirectCubeMapShader = new Shader("EquirectCubeMap", "shader/standard_mvp.vert", "shader/equirect_cubemap.frag", MaterialType::EquirectCubeMap);
  auto litShader = new LitShader("Lit", "shader/lit.vert", "shader/lit.frag");
  auto skyboxShader = new Shader("Skybox", "shader/skybox.vert", "shader/skybox.frag", MaterialType::Skybox);
  auto unlitShader = new UnlitShader("Unlit", "shader/unlit.vert", "shader/unlit.frag");
  computes.insert({brdfCompute->GetType(), brdfCompute});
  shaders.insert({bloomShader->GetType(), bloomShader});
  shaders.insert({blurShader->GetType(), blurShader});
  shaders.insert({brightShader->GetType(), brightShader});
  shaders.insert({cubeMapEquirectShader->GetType(), cubeMapEquirectShader});
  shaders.insert({cubeMapIrradianceShader->GetType(), cubeMapIrradianceShader});
  shaders.insert({cubeMapPrefilterShader->GetType(), cubeMapPrefilterShader});
  shaders.insert({equirectCubeMapShader->GetType(), equirectCubeMapShader});
  shaders.insert({litShader->GetType(), litShader});
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
  glDeleteVertexArrays(1, &materialVBO);
  glDeleteVertexArrays(1, &transformVBO);
  for (const auto& [_, compute] : computes)
    delete compute;
  for (const auto& [_, shader] : shaders)
    delete shader;
  computes.clear();
  shaders.clear();
  framebufferPool.Clear();
  renderbufferPool.Clear();
  texturePool.Clear();
}
int RenderingSystem::GetFPS() const {
  return fps;
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
  case MaterialType::Bloom:
    return shaders[MaterialType::Bloom];
  case MaterialType::Bright:
    return shaders[MaterialType::Bright];
  case MaterialType::Blur:
    return shaders[MaterialType::Blur];
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
