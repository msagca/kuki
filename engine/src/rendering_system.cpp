#define GLM_ENABLE_EXPERIMENTAL
#include <application.hpp>
#include <asset_type.hpp>
#include <camera.hpp>
#include <cstdint>
#include <embedded_shaders.hpp>
#include <gl_renderer.hpp>
#include <id.hpp>
#include <material_type.hpp>
#include <memory>
#include <queue>
#include <render_pass.hpp>
#include <render_target.hpp>
#include <renderer.hpp>
#include <rendering_system.hpp>
#include <string>
#include <system.hpp>
#include <target_description.hpp>
#include <utility>
namespace kuki {
RenderingSystem::RenderingSystem(Application &app)
  : System(std::in_place_type<RenderingSystem>, app) {}
auto RenderingSystem::Start() -> void {
  SetRenderer();
  using namespace embedded_shader;
  app.LoadComputeFromSource(brdf_lut_comp, "BRDF_LUT");
  app.LoadComputeFromSource(cubemap_equirect_comp, "CubemapEquirect");
  app.LoadComputeFromSource(equirect_cubemap_comp, "EquirectCubemap");
  app.LoadComputeFromSource(irradiance_comp, "IrradianceMap");
  app.LoadComputeFromSource(prefilter_comp, "PrefilterMap");
  app.LoadComputeFromSource(sh_project_comp, "SHProject");
  app.LoadShaderFromSource(lit_vert, lit_frag, "Lit", MaterialType::Lit);
  app.LoadShaderFromSource(lit_skinned_vert, lit_frag, "LitSkinned", MaterialType::Lit);
  app.LoadShaderFromSource(skybox_vert, skybox_frag, "Skybox");
  app.LoadShaderFromSource(standard_instanced_vert, empty_frag, "ShadowMap");
  app.LoadShaderFromSource(standard_m_vert, bloom_frag, "Bloom");
  app.LoadShaderFromSource(standard_m_vert, blur_frag, "Blur");
  app.LoadShaderFromSource(standard_m_vert, bright_pass_frag, "BrightPass");
  app.LoadShaderFromSource(standard_m_vert, gamma_correction_frag, "GammaCorrect");
  app.LoadShaderFromSource(standard_m_vert, outline_frag, "Outline");
  app.LoadShaderFromSource(unlit_vert, unlit_frag, "Unlit");
  if (activeRenderer)
    activeRenderer->LoadAssets(AssetType::Shader);
  if (activeRenderer)
    activeRenderer->LoadAssets(AssetType::Mesh);
  renderGraph = graphBuilder
                  .BeginGraph()
                  .BeginPass(RenderPass::ShadowMap)
                  .AddOutput("ShadowMap", {.format = TargetFormat::DEPTH, .width = screenWidth * 4, .height = screenHeight * 4})
                  .EndPass()
                  .BeginPass(RenderPass::SpotShadowMap)
                  .AddOutput("SpotShadowMap", {.format = TargetFormat::DEPTH, .type = TargetType::Texture2DArray, .width = screenWidth, .height = screenHeight, .layers = static_cast<int>(MAX_SPOT_SHADOW_LIGHTS)})
                  .EndPass()
                  .BeginPass(RenderPass::Scene)
                  .AddInput("ShadowMap")
                  .AddInput("SpotShadowMap")
                  .AddOutput("SceneMulti", {.type = TargetType::Texture2DMulti, .width = screenWidth, .height = screenHeight, .samples = 4, .pickingBuffer = true})
                  .EndPass()
                  .BeginPass(RenderPass::AntiAliasing)
                  .AddInput("SceneMulti")
                  .AddOutput("Scene", {.width = screenWidth, .height = screenHeight, .samples = 1})
                  .EndPass()
                  .BeginPass(RenderPass::Outline)
                  .AddInput("SceneMulti")
                  .AddInput("Scene")
                  .AddOutput("SceneOutlined", {.width = screenWidth, .height = screenHeight, .samples = 1})
                  .EndPass()
                  .BeginPass(RenderPass::GammaCorrection)
                  .AddInput("SceneOutlined")
                  .AddOutput("SceneSRGB")
                  .EndPass()
                  .EndGraph();
  if (renderGraph)
    renderGraph->Compile();
}
auto RenderingSystem::Update(float deltaTime) -> void {
  static std::queue<float> times;
  static auto accumulatedTime = 0.f;
  times.push(deltaTime);
  accumulatedTime += deltaTime;
  while (accumulatedTime > 1.f && !times.empty()) {
    auto lastTime = times.front();
    accumulatedTime -= lastTime;
    times.pop();
  }
  fps = times.size();
  auto scene = app.GetScene();
  if (!scene)
    return;
  scene->UpdateComponents<Camera>();
  if (activeRenderer) {
    activeRenderer->LoadScene(*scene);
    if (renderGraph)
      renderGraph->Execute(*activeRenderer);
  }
}
auto RenderingSystem::Shutdown() -> void {
  for (auto &renderer : renderers)
    if (renderer)
      renderer->Clear();
}
auto RenderingSystem::GetFPS() const -> size_t {
  return fps;
}
auto RenderingSystem::GetResolution() const -> std::pair<int, int> {
  return {screenWidth, screenHeight};
}
auto RenderingSystem::GetTarget(std::string name) -> RenderTarget * {
  if (!activeRenderer)
    return nullptr;
  if (name.empty() && renderGraph)
    name = renderGraph->GetFinalOutputName();
  return activeRenderer->GetTarget(name);
}
auto RenderingSystem::PickEntity(const int x, const int y) -> EntityID {
  return activeRenderer ? activeRenderer->PickEntity(x, y) : EntityID::Invalid;
}
auto RenderingSystem::LoadAssets(const AssetType type) -> void {
  if (activeRenderer)
    activeRenderer->LoadAssets(type);
}
auto RenderingSystem::GetPreviewSize() const -> int {
  return activeRenderer ? activeRenderer->GetPreviewSize() : 0;
}
auto RenderingSystem::PreviewAsset(const AssetID assetId) -> RenderTarget * {
  return activeRenderer->PreviewAsset(assetId);
}
auto RenderingSystem::SetPreviewSize(const int size) -> void {
  if (activeRenderer)
    activeRenderer->SetPreviewSize(size);
}
auto RenderingSystem::SetRenderer(const RenderingAPI api) -> void {
  switch (api) {
  case RenderingAPI::DirectX:
    break;
  case RenderingAPI::Vulkan:
    break;
  default:
    const auto index = static_cast<uint8_t>(RenderingAPI::OpenGL);
    if (!renderers.at(index))
      renderers[index] = std::make_unique<GLRenderer>(app);
    activeRenderer = renderers[index].get();
  }
}
auto RenderingSystem::SetResolution(const int width, const int height) -> void {
  if (width <= 0 || height <= 0)
    return;
  if (auto scene = app.GetScene(); scene)
    if (auto camera = scene->GetCamera(); camera) {
      const auto aspectRatio = static_cast<float>(width) / height;
      if (camera->aspectRatio != aspectRatio) {
        camera->aspectRatio = aspectRatio;
        ++camera->dirty;
      }
    }
  if (width == screenWidth && height == screenHeight)
    return;
  screenWidth = width;
  screenHeight = height;
  if (!activeRenderer)
    return;
  renderGraph->ResizeTargets(*activeRenderer, screenWidth, screenHeight);
}
} // namespace kuki
