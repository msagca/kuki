#define GLM_ENABLE_EXPERIMENTAL
#include <application.hpp>
#include <array>
#include <asset_type.hpp>
#include <camera.hpp>
#include <cstdint>
#include <cstdlib>
#include <embedded_shaders.hpp>
#include <gl_renderer.hpp>
#ifdef KUKI_HAS_DIRECTX
#include <dx_renderer.hpp>
#endif
#include <id.hpp>
#include <light_limits.hpp>
#include <material_type.hpp>
#include <memory>
#include <profiler.hpp>
#include <queue>
#include <render_pass.hpp>
#include <render_target.hpp>
#include <renderer.hpp>
#include <engine_config.hpp>
#include <rendering_system.hpp>
#include <spdlog/spdlog.h>
#include <light.hpp>
#include <skybox_handle.hpp>
#include <skybox_light.hpp>
#include <texture.hpp>
#include <texture_asset.hpp>
#include <scene.hpp>
#include <string>
#include <system.hpp>
#include <target_description.hpp>
#include <utility>
namespace kuki {
RenderingSystem::RenderingSystem(Application &app)
  : System(std::in_place_type<RenderingSystem>, app) {}
auto RenderingSystem::Start() -> void {
  SetRenderer(app.GetDescription().api);
  // The other half of the installation's display preference, applied here rather than left to each
  // application to remember. The editor also sets it in its own `Start`, which is now a no-op that
  // repeats this -- but it is the thing that lets the panel change it afterwards, so it stays.
  SetToneMapper(EngineConfig::Load().toneMapper);
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
  app.LoadShaderFromSource(standard_m_vert, tone_mapping_frag, "ToneMapping");
  app.LoadShaderFromSource(standard_m_vert, outline_frag, "Outline");
  app.LoadShaderFromSource(standard_m_vert, pick_frag, "Pick");
  app.LoadShaderFromSource(unlit_vert, unlit_frag, "Unlit");
  app.LoadShaderFromSource(overlay_vert, overlay_frag, "Overlay");
  if (activeRenderer)
    activeRenderer->LoadAssets(AssetType::Shader);
  if (activeRenderer)
    activeRenderer->LoadAssets(AssetType::Mesh);
  const auto bloomWidth = std::max(1, screenWidth / 2);
  const auto bloomHeight = std::max(1, screenHeight / 2);
  renderGraph = graphBuilder
                  .BeginGraph()
                  .BeginPass(RenderPass::ShadowMap)
                  .AddOutput("ShadowMap", {.format = TargetFormat::DEPTH, .width = SHADOW_MAP_RESOLUTION, .height = SHADOW_MAP_RESOLUTION}, TargetSizing::Fixed)
                  .EndPass()
                  .BeginPass(RenderPass::SpotShadowMap)
                  .AddOutput("SpotShadowMap", {.format = TargetFormat::DEPTH, .type = TargetType::Texture2DArray, .width = SPOT_SHADOW_MAP_RESOLUTION, .height = SPOT_SHADOW_MAP_RESOLUTION, .layers = static_cast<int>(MAX_SPOT_SHADOW_LIGHTS)}, TargetSizing::Fixed)
                  .EndPass()
                  // Depth alone, before anything is shaded, and read by nobody since the screen
                  // space occlusion pass it was added for was removed. It is not the early-z the
                  // name suggests either: the scene pass renders against the depth buffer its own
                  // multisampled target carries, not this one. Kept because the debug view list is
                  // the one place a depth buffer can be looked at, and because an occlusion term
                  // that is traced rather than gathered off the screen would want it back.
                  .BeginPass(RenderPass::DepthPrepass)
                  .AddOutput("SceneDepth", {.format = TargetFormat::DEPTH, .width = screenWidth, .height = screenHeight})
                  .EndPass()
                  // Sixty-four rays from every probe, shaded and averaged into the field the scene
                  // pass reads its bounce out of. A pass like any other, and here for the reason any
                  // of them are: what it fills is an input to shading, so it has to be finished
                  // before shading starts, and the graph is where that is said.
                  //
                  // `AddResource` rather than `AddOutput` because the field is a structured buffer
                  // the backend owns across frames, not a viewport-sized target the graph allocates.
                  // The graph orders on the name without ever holding the thing behind it.
                  .BeginPass(RenderPass::ProbeTrace)
                  .AddResource("ProbeField")
                  .EndPass()
                  .BeginPass(RenderPass::Scene)
                  .AddInput("ShadowMap")
                  .AddInput("SpotShadowMap")
                  .AddInput("ProbeField")
                  .AddOutput("SceneMulti", {.type = TargetType::Texture2DMulti, .width = screenWidth, .height = screenHeight, .samples = 4, .pickingBuffer = true})
                  .EndPass()
                  .BeginPass(RenderPass::AntiAliasing)
                  .AddInput("SceneMulti")
                  .AddOutput("Scene", {.width = screenWidth, .height = screenHeight, .samples = 1})
                  .EndPass()
                  .BeginPass(RenderPass::BrightPassFilter)
                  .AddInput("Scene")
                  .AddOutput("SceneBright", {.width = bloomWidth, .height = bloomHeight, .samples = 1}, TargetSizing::ViewportHalf)
                  .EndPass()
                  .BeginPass(RenderPass::BlurEffect)
                  .AddInput("SceneBright")
                  .AddOutput("SceneBlurred", {.width = bloomWidth, .height = bloomHeight, .samples = 1}, TargetSizing::ViewportHalf)
                  .AddOutput("SceneBlurredPing", {.width = bloomWidth, .height = bloomHeight, .samples = 1}, TargetSizing::ViewportHalf)
                  .AddOutput("SceneBlurredPong", {.width = bloomWidth, .height = bloomHeight, .samples = 1}, TargetSizing::ViewportHalf)
                  .EndPass()
                  .BeginPass(RenderPass::BloomEffect)
                  .AddInput("Scene")
                  .AddInput("SceneBlurred")
                  .AddOutput("SceneBloom", {.width = screenWidth, .height = screenHeight, .samples = 1})
                  .EndPass()
                  .BeginPass(RenderPass::Outline)
                  .AddInput("SceneMulti")
                  .AddInput("SceneBloom")
                  .AddOutput("SceneOutlined", {.width = screenWidth, .height = screenHeight, .samples = 1})
                  .EndPass()
                  .BeginPass(RenderPass::ToneMapping)
                  .AddInput("SceneOutlined")
                  .AddOutput("SceneSRGB", {.width = screenWidth, .height = screenHeight, .samples = 1})
                  .EndPass()
                  // Last, so the text is laid over a picture that is finished being a picture. A
                  // caption drawn before tone mapping would be exposed and mapped along with the
                  // scene, which is to say it would change brightness according to how bright the
                  // room it is captioning happens to be.
                  //
                  // It costs a copy in every frame that draws no text, since the graph gives each
                  // pass its own output and the one after this reads what this leaves. That is one
                  // more full-screen blit in a chain that already has several, and the alternative
                  // -- a pass writing over its own input -- is the one thing the graph cannot
                  // order safely.
                  .BeginPass(RenderPass::Overlay)
                  .AddInput("SceneSRGB")
                  .AddOutput("SceneOverlay", {.width = screenWidth, .height = screenHeight, .samples = 1})
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
  ++frameCounter;
  KUKI_PROFILE_SCOPE("Rendering");
  auto scene = app.GetScene();
  if (!scene) {
    // Drained here as well as at the end, so that a frame with nothing to draw into still finishes
    // the queue rather than letting it accumulate. See the note at the bottom of this function.
    app.GetOverlay().Clear();
    return;
  }
  // Transforms as well as cameras, and not only because it is convenient here. `UpdateTransforms`
  // used to be reached from `PhysicsSystem::Update` alone, which made drawing the right picture
  // depend on a system a game has every reason to leave out: `SystemApplication` takes its systems
  // as template parameters, and an application without physics got no transform resolution at all
  // -- every entity drawn from an identity `world` matrix, the whole scene stacked at the origin.
  //
  // It belongs here regardless of that. The world matrices are an input to drawing in the same way
  // the camera's view matrix is, they have to be current at this exact point in the frame, and the
  // dirty tracking in `EntityManager` makes a second flush after physics almost free.
  scene->UpdateComponents<Transform, Camera>();
  // Render at the window's size, asked for every frame rather than once. `SetResolution` debounces:
  // a new size becomes a candidate and is only committed once the same size has been asked for
  // several frames running, so a single request at startup is recorded and never applied. Asking
  // every frame also means a resized window needs no callback of its own.
  if (presentEnabled)
    if (const auto *context = app.GetGraphicsContext(); context) {
      const auto [surfaceWidth, surfaceHeight] = context->GetSurfaceSize();
      if (surfaceWidth > 0 && surfaceHeight > 0)
        SetResolution(surfaceWidth, surfaceHeight);
    }
  UpdateSkyboxLight(*scene);
  // The component is where the values live, and the renderer is where they are read from. Pushed
  // every frame rather than on a change, because a component is edited by whoever holds a pointer to
  // it and there is nothing to notice a change -- and a struct this size copied once a frame is
  // cheaper than any mechanism that would.
  //
  // A scene with no such component leaves the renderer holding what it had, which is what makes this
  // safe to skip: the defaults are already there from construction, and a backend swapped at runtime
  // carries its settings across because they never stopped living on `Renderer`.
  if (activeRenderer) {
    if (const auto *settings = scene->GetAnyComponent<IndirectLighting>())
      activeRenderer->SetIndirectLighting(*settings);
    // From the active camera, so that switching cameras switches the view. The renderer keeps
    // reading its own member, which is now a copy of whichever camera is currently looking.
    if (const auto *camera = scene->GetCamera()) {
      activeRenderer->SetLightingDebugView(camera->lightingDebugView);
      activeRenderer->SetProbeDebugView(camera->probeDebugView);
    }
  }
  if (activeRenderer) {
    {
      KUKI_PROFILE_SCOPE("LoadScene");
      activeRenderer->LoadScene(*scene);
    }
    if (renderGraph)
      renderGraph->Execute(*activeRenderer);
    if (presentEnabled && renderGraph) {
      KUKI_PROFILE_SCOPE("PresentTarget");
      activeRenderer->PresentTarget(renderGraph->GetFinalOutputName());
    }
  }
  // Emptied by the frame that drew it, which is what makes the overlay immediate mode: a caption
  // appears for exactly as long as something keeps asking for it, and there is no handle to hold
  // or to forget to release. Here rather than at the top of the frame so that a renderer which
  // never ran -- no scene, no backend -- does not silently swallow what was queued for it.
  app.GetOverlay().Clear();
}
auto RenderingSystem::IsPresentEnabled() const -> bool {
  return presentEnabled;
}
auto RenderingSystem::SetPresentEnabled(const bool enabled) -> void {
  presentEnabled = enabled;
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
auto RenderingSystem::GetLightingDebugView() const -> LightingDebugView {
  return activeRenderer ? activeRenderer->GetLightingDebugView() : LightingDebugView::None;
}
auto RenderingSystem::SetLightingDebugView(const LightingDebugView view) -> void {
  if (activeRenderer)
    activeRenderer->SetLightingDebugView(view);
}
auto RenderingSystem::IsPassEnabled(const RenderPass pass) const -> bool {
  // True with no renderer, because the question is what the pipeline is meant to do rather than
  // what it managed to do, and every pass is meant to run until somebody says otherwise.
  return activeRenderer ? activeRenderer->IsPassEnabled(pass) : true;
}
auto RenderingSystem::SetPassEnabled(const RenderPass pass, const bool enabled) -> void {
  if (activeRenderer)
    activeRenderer->SetPassEnabled(pass, enabled);
}
auto RenderingSystem::GetPoolUsage() const -> PoolUsage {
  return activeRenderer ? activeRenderer->GetPoolUsage() : PoolUsage{};
}
auto RenderingSystem::GetProbeDebugView() const -> ProbeDebugView {
  return activeRenderer ? activeRenderer->GetProbeDebugView() : ProbeDebugView::Off;
}
auto RenderingSystem::SetProbeDebugView(const ProbeDebugView view) -> void {
  if (activeRenderer)
    activeRenderer->SetProbeDebugView(view);
}
auto RenderingSystem::GetIndirectLighting() const -> const IndirectLighting & {
  // The defaults, when there is no renderer to ask. A reference has to refer to something, and the
  // honest something is the values a renderer would have started with.
  static constexpr IndirectLighting fallback{};
  return activeRenderer ? activeRenderer->GetIndirectLighting() : fallback;
}
auto RenderingSystem::SetIndirectLighting(const IndirectLighting &settings) -> void {
  if (activeRenderer)
    activeRenderer->SetIndirectLighting(settings);
}
auto RenderingSystem::GetCapabilities() const -> RendererCapabilities {
  return activeRenderer ? activeRenderer->GetCapabilities() : RendererCapabilities{};
}
auto RenderingSystem::GetToneMapper() const -> ToneMapper {
  return activeRenderer ? activeRenderer->GetToneMapper() : DEFAULT_TONE_MAPPER;
}
auto RenderingSystem::SetToneMapper(const ToneMapper mapper) -> void {
  if (activeRenderer)
    activeRenderer->SetToneMapper(mapper);
}
auto RenderingSystem::GetUploadBudget() const -> size_t {
#ifdef KUKI_HAS_DIRECTX
  if (auto *dx = dynamic_cast<DXRenderer *>(activeRenderer); dx)
    return dx->GetUploadBudget();
#endif
  return 0;
}
auto RenderingSystem::SetUploadBudget(const size_t bytes) -> void {
#ifdef KUKI_HAS_DIRECTX
  if (auto *dx = dynamic_cast<DXRenderer *>(activeRenderer); dx)
    dx->SetUploadBudget(bytes);
#else
  (void)bytes;
#endif
}
auto RenderingSystem::SetRenderer(const RenderingAPI api) -> void {
  switch (api) {
#ifdef KUKI_HAS_DIRECTX
  case RenderingAPI::DirectX: {
    const auto index = static_cast<uint8_t>(RenderingAPI::DirectX);
    if (!renderers.at(index))
      renderers[index] = std::make_unique<DXRenderer>(app);
    activeRenderer = renderers[index].get();
    break;
  }
#else
  case RenderingAPI::DirectX:
    spdlog::warn("[RenderingSystem] DirectX is not available in this build, falling back to OpenGL");
    [[fallthrough]];
#endif
  case RenderingAPI::Vulkan:
    spdlog::warn("[RenderingSystem] Vulkan is not implemented, falling back to OpenGL");
    [[fallthrough]];
  default:
    const auto index = static_cast<uint8_t>(RenderingAPI::OpenGL);
    if (!renderers.at(index))
      renderers[index] = std::make_unique<GLRenderer>(app);
    activeRenderer = renderers[index].get();
  }
}
auto RenderingSystem::UpdateSkyboxLight(Scene &scene) -> void {
  AssetID skyboxAsset{};
  EntityID skyboxEntity{};
  scene.ForEachEntity<SkyboxHandle>([&](const EntityID id, const SkyboxHandle *handle) {
    if (skyboxAsset || !handle->assetId)
      return;
    skyboxAsset = handle->assetId;
    skyboxEntity = id;
  });
  if (!skyboxAsset || skyboxAsset == derivedSkyboxAsset || !app.IsAssetLoaded(skyboxAsset))
    return;
  auto textureAsset = app.GetAsset<TextureAsset>(skyboxAsset);
  if (!textureAsset)
    return;
  derivedSkyboxAsset = skyboxAsset;
  const auto hadPixels = HasTexturePixels(textureAsset->texture);
  EnsureTexturePixels(textureAsset->texture);
  const auto dominant = ExtractDominantLight(textureAsset->texture);
  if (!hadPixels)
    ReleaseTexturePixels(textureAsset->texture);
  if (!dominant)
    return;
  auto light = scene.GetEntityComponent<Light>(skyboxEntity);
  if (!light) {
    scene.AddEntityComponent<Light>(skyboxEntity);
    light = scene.GetEntityComponent<Light>(skyboxEntity);
  }
  if (!light)
    return;
  light->type = LightType::Directional;
  light->forward = dominant->direction;
  light->diffuse = dominant->color;
  light->specular = dominant->color;
  spdlog::info("[RenderingSystem] Adopted a directional light from the skybox");
}
auto RenderingSystem::NearlyEqual(const int a, const int b) -> bool {
  return std::abs(a - b) < RESOLUTION_CHANGE_THRESHOLD;
}
auto RenderingSystem::CommitResolution(const int width, const int height) -> void {
  screenWidth = width;
  screenHeight = height;
  if (activeRenderer && renderGraph)
    renderGraph->ResizeTargets(*activeRenderer, screenWidth, screenHeight);
  spdlog::debug("[RenderingSystem] Resolution now {}x{}", screenWidth, screenHeight);
}
auto RenderingSystem::UpdatePendingResolution(const int width, const int height) -> void {
  if (NearlyEqual(width, screenWidth) && NearlyEqual(height, screenHeight)) {
    hasPending = false;
    return;
  }
  if (!hasPending || !NearlyEqual(width, pendingWidth) || !NearlyEqual(height, pendingHeight)) {
    hasPending = true;
    pendingWidth = width;
    pendingHeight = height;
    pendingSinceFrame = frameCounter;
    return;
  }
  if (frameCounter - pendingSinceFrame < RESOLUTION_DEBOUNCE_FRAMES)
    return;
  hasPending = false;
  CommitResolution(pendingWidth, pendingHeight);
}
auto RenderingSystem::SetResolution(const int width, const int height) -> void {
  if (width > 0 && height > 0)
    UpdatePendingResolution(width, height);
  if (auto scene = app.GetScene(); scene)
    if (auto camera = scene->GetCamera(); camera) {
      const auto aspectRatio = static_cast<float>(screenWidth) / screenHeight;
      if (camera->aspectRatio != aspectRatio) {
        camera->aspectRatio = aspectRatio;
        ++camera->dirty;
      }
    }
}
} // namespace kuki
