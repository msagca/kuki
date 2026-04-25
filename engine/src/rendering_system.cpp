#define GLM_ENABLE_EXPERIMENTAL
#include <application.hpp>
#include <application_settings.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <entity_manager.hpp>
#include <enum_traits.hpp>
#include <gl_mesh_material.hpp>
#include <gl_renderer.hpp>
#include <gl_shader.hpp>
#include <glm/detail/type_vec3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <id.hpp>
#include <keyed_pool.hpp>
#include <light.hpp>
#include <primitive.hpp>
#include <renderer.hpp>
#include <rendering_system.hpp>
#include <settings_manager.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <system.hpp>
#include <target_description.hpp>
#include <texture_pool.hpp>
#include <transform.hpp>
//
#include <glad/glad.h>
namespace kuki {
RenderingSystem::RenderingSystem(SceneManager &sceneManager, AssetManager &assetManager, SettingsManager &settingsManager)
  : System(std::in_place_type<RenderingSystem>), sceneManager(sceneManager), assetManager(assetManager), settingsManager(settingsManager), glRenderer(sceneManager, assetManager) {}
RenderingSystem::~RenderingSystem() {}
auto RenderingSystem::Start() -> void {
  activeRenderer = &glRenderer;
  sceneManager.OnSceneLoaded += [this](Scene &scene) { OnSceneLoaded(scene); };
  settingsManager.OnResolutionChanged += [this](const ScreenResolution &res) { OnResolutionChanged(res); };
  const auto &res = settingsManager.GetResolution();
  renderGraph = graphBuilder
                  .BeginGraph()
                  .BeginPass(RenderScene)
                  // NOTE: unless overriden, subsequent outputs will use this description (no partial overrides)
                  .AddOutput("SceneMulti", {.type = TargetType::Texture2DMulti, .width = res.width, .height = res.height, .samples = 4})
                  .EndPass()
                  .BeginPass(ApplyAntiAliasing)
                  .AddInput("SceneMulti")
                  .AddOutput("Scene", {.width = res.width, .height = res.height, .samples = 1})
                  .EndPass()
                  // .BeginPass(ApplyBlurEffect)
                  // .AddInput("Scene")
                  // .AddOutput("SceneBlur")
                  // .EndPass()
                  // .BeginPass(ApplyBrightPassFilter)
                  // .AddInput("SceneBlur")
                  // .AddOutput("SceneBright")
                  // .EndPass()
                  // .BeginPass(ApplyBloomEffect)
                  // .AddInput("Scene")
                  // .AddInput("SceneBright")
                  // .AddOutput("SceneBloom")
                  // .EndPass()
                  .BeginPass(ApplyGammaCorrection)
                  .AddInput("Scene")
                  .AddOutput("SceneSRGB")
                  .EndPass()
                  .EndGraph();
  // TODO: move these to somewhere more appropriate
  assetManager.ForEach<MeshAsset>([this](const AssetID id, const std::string &) {
    activeRenderer->LoadAsset(id);
  });
  assetManager.ForEach<MaterialAsset>([this](const AssetID id, const std::string &) {
    activeRenderer->LoadAsset(id);
  });
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
  // TODO: move this to somewhere more appropriate
  assetManager.ForEach<ShaderAsset>([this](const AssetID id, const std::string &) {
    activeRenderer->LoadAsset(id);
  });
  auto scene = sceneManager.GetActive();
  if (!scene)
    return;
  scene->UpdateComponents<Camera>();
  if (renderGraph && activeRenderer)
    renderGraph->Execute(*activeRenderer);
}
auto RenderingSystem::Shutdown() -> void {
  if (activeRenderer)
    activeRenderer->Clear();
  if (activeRenderer != &glRenderer)
    glRenderer.Clear();
}
auto RenderingSystem::GetFPS() const -> size_t {
  return fps;
}
auto RenderingSystem::GetTarget(std::string name) -> RenderTarget * {
  if (!activeRenderer)
    return nullptr;
  if (name.empty() && renderGraph)
    name = renderGraph->GetFinalOutputName();
  return activeRenderer->GetTarget(name);
}
auto RenderingSystem::LoadAsset(const AssetID id) -> void {
  if (activeRenderer)
    activeRenderer->LoadAsset(id);
}
auto RenderingSystem::PreviewAsset(const AssetID assetId, int size) -> RenderTarget * {
  return activeRenderer->PreviewAsset(assetId);
}
auto RenderingSystem::OnResolutionChanged(const ScreenResolution &res) -> void {
  auto scene = sceneManager.GetActive();
  if (!scene)
    return;
  auto camera = scene->GetActiveCamera();
  if (!camera)
    return;
  camera->aspectRatio = static_cast<float>(res.width) / res.height;
  ++camera->dirty;
  if (!activeRenderer)
    return;
  renderGraph->ResizeTargets(*activeRenderer, res.width, res.height);
  if (!activeRenderer->Is<GLRenderer>())
    glViewport(0, 0, res.width, res.height);
}
auto RenderingSystem::OnSceneLoaded(Scene &scene) -> void {
  auto camera = scene.GetActiveCamera();
  if (!camera)
    return;
  const auto &res = settingsManager.GetResolution();
  camera->aspectRatio = static_cast<float>(res.width) / res.height;
  ++camera->dirty;
  if (!activeRenderer)
    return;
  auto glRenderer = activeRenderer->As<GLRenderer>();
  if (!glRenderer)
    return;
  glRenderer->LoadScene(scene);
  glViewport(0, 0, res.width, res.height);
  spdlog::info("[OpenGL] loaded scene: {}", sceneManager.GetName(scene.id));
}
auto RenderingSystem::ApplyAntiAliasing(Renderer &renderer, std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  if (inputs.size() != 1 || outputs.size() != 1)
    return;
  auto glRenderer = renderer.As<GLRenderer>();
  if (!glRenderer)
    return;
  const auto in = glRenderer->GetTarget(inputs[0]);
  const auto out = glRenderer->GetTarget(outputs[0]);
  if (!in || !out)
    return;
  glBindFramebuffer(GL_READ_FRAMEBUFFER, in->framebuffer);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, out->framebuffer);
  glBlitFramebuffer(0, 0, out->desc.width, out->desc.height, 0, 0, out->desc.width, out->desc.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto RenderingSystem::ApplyBloomEffect(Renderer &renderer, std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  if (inputs.size() != 2 || outputs.size() != 1)
    return;
  auto glRenderer = renderer.As<GLRenderer>();
  if (!glRenderer)
    return;
  auto bloomShader = glRenderer->GetShader("Bloom");
  if (!bloomShader)
    return;
  const auto mesh = glRenderer->GetPrimitive("Frame");
  if (!mesh)
    return;
  const auto in0 = glRenderer->GetTarget(inputs[0]);
  const auto in1 = glRenderer->GetTarget(inputs[1]);
  const auto out = glRenderer->GetTarget(outputs[0]);
  if (!in0 || !in1 || !out)
    return;
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  bloomShader->Use();
  bloomShader->SetTexture("image", in0->texture);
  bloomShader->SetTexture("imageBright", in1->texture);
  bloomShader->SetUniform("intensity", .5f);
  bloomShader->SetUniform("model", glm::mat4(1.f));
  bloomShader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto RenderingSystem::ApplyBlurEffect(Renderer &renderer, std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  constexpr auto NUM_PASSES = 8;
  if (inputs.size() != 1 || outputs.size() != 1)
    return;
  auto glRenderer = renderer.As<GLRenderer>();
  if (!glRenderer)
    return;
  auto blurShader = glRenderer->GetShader("Blur");
  if (!blurShader)
    return;
  const auto mesh = glRenderer->GetPrimitive("Frame");
  if (!mesh)
    return;
  const auto in = glRenderer->GetTarget(inputs[0]);
  const auto out = glRenderer->GetTarget(outputs[0]);
  const auto ping = glRenderer->GetTarget(outputs[0] + "Ping");
  const auto pong = glRenderer->GetTarget(outputs[0] + "Pong");
  if (!in || !out || !ping || !pong)
    return;
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glBindFramebuffer(GL_FRAMEBUFFER, ping->framebuffer);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glBindFramebuffer(GL_FRAMEBUFFER, pong->framebuffer);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  blurShader->Use();
  blurShader->SetUniform("model", glm::mat4(1.f));
  for (auto i = 0; i < NUM_PASSES; ++i) {
    const auto even = i % 2 == 0;
    const auto &srcTexture = i == 0 ? in->texture : even ? pong->texture
                                                         : ping->texture;
    const auto &dstFramebuffer = i == NUM_PASSES - 1 ? out->framebuffer : even ? ping->framebuffer
                                                                               : pong->framebuffer;
    /* pass src  dst
     * 0    in   ping
     * 1    ping pong
     * 2    pong ping
     * 3    ping pong
     * 4    pong ping
     * 5    ping pong
     * 6    pong ping
     * 7    ping out
     */
    glBindFramebuffer(GL_FRAMEBUFFER, dstFramebuffer);
    blurShader->SetTexture("image", srcTexture);
    blurShader->SetUniform("horizontal", even);
    blurShader->Draw(*mesh);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto RenderingSystem::ApplyBrightPassFilter(Renderer &renderer, std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  if (inputs.size() != 1 || outputs.size() != 1)
    return;
  auto glRenderer = renderer.As<GLRenderer>();
  if (!glRenderer)
    return;
  auto brightShader = glRenderer->GetShader("BrightPass");
  if (!brightShader)
    return;
  const auto mesh = glRenderer->GetPrimitive("Frame");
  if (!mesh)
    return;
  const auto in = glRenderer->GetTarget(inputs[0]);
  const auto out = glRenderer->GetTarget(outputs[0]);
  if (!in || !out)
    return;
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  brightShader->Use();
  brightShader->SetTexture("image", in->texture);
  brightShader->SetUniform("threshold", .5f);
  brightShader->SetUniform("model", glm::mat4(1.f));
  brightShader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto RenderingSystem::ApplyGammaCorrection(Renderer &renderer, std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  if (inputs.size() != 1 || outputs.size() != 1)
    return;
  auto glRenderer = renderer.As<GLRenderer>();
  if (!glRenderer)
    return;
  auto gammaShader = glRenderer->GetShader("GammaCorrect");
  if (!gammaShader)
    return;
  const auto mesh = glRenderer->GetPrimitive("Frame");
  if (!mesh)
    return;
  const auto in = glRenderer->GetTarget(inputs[0]);
  const auto out = glRenderer->GetTarget(outputs[0]);
  if (!in || !out)
    return;
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  gammaShader->Use();
  gammaShader->SetTexture("image", in->texture);
  gammaShader->SetUniform("gamma", 2.2f);
  gammaShader->SetUniform("model", glm::mat4(1.f));
  gammaShader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto RenderingSystem::RenderScene(Renderer &renderer, std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  if (outputs.size() != 1)
    return;
  auto glRenderer = renderer.As<GLRenderer>();
  if (!glRenderer)
    return;
  auto scene = glRenderer->GetScene();
  if (!scene)
    return;
  const auto out = glRenderer->GetTarget(outputs[0]);
  if (!out)
    return;
  glRenderer->LoadScene(*scene); // TODO: optimize this
  glEnable(GL_DEPTH_TEST);
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  glViewport(0, 0, out->desc.width, out->desc.height);
  DrawSkybox(renderer);
  DrawEntities(renderer);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_DEPTH_TEST); // disable before post-processing
}
auto RenderingSystem::DrawEntities(Renderer &renderer) -> void {
  auto scene = renderer.GetScene();
  if (!scene)
    return;
  // TODO: cache these
  std::unordered_map<GLMeshMat, std::vector<glm::mat4>> meshToMatToTransforms;
  std::unordered_map<GLMeshMat, std::vector<MaterialFallback>> meshToMatToFallbacks;
  // TODO: use ForEachVisibleEntity below
  scene->ForEachEntity<GLMesh, GLMaterial, Transform>([&](const EntityID id, const GLMesh *mesh, const GLMaterial *material, const Transform *transform) {
    if (mesh->vao == 0)
      return;
    GLMeshMat meshMat{.mesh = *mesh, .material = *material};
    meshToMatToTransforms[meshMat].push_back(transform->world);
    meshToMatToFallbacks[meshMat].push_back(material->fallback);
  });
  for (const auto &[meshMat, transforms] : meshToMatToTransforms)
    DrawEntitiesInstanced(renderer, meshMat.mesh, meshMat.material, meshToMatToFallbacks[meshMat], transforms);
}
auto RenderingSystem::DrawEntitiesInstanced(Renderer &renderer, const GLMesh &mesh, const GLMaterial &material, const std::vector<MaterialFallback> &fallbacks, const std::vector<glm::mat4> &transforms) -> void {
  auto glRenderer = renderer.As<GLRenderer>();
  if (!glRenderer)
    return;
  auto scene = glRenderer->GetScene();
  if (!scene)
    return;
  auto camera = scene->GetActiveCamera();
  if (!camera)
    return;
  auto shader = glRenderer->GetShader(material.type);
  if (!shader)
    return;
  const auto materialBufferId = glRenderer->CreateBuffer("MaterialBuffer");
  const auto transformBufferId = glRenderer->CreateBuffer("TransformBuffer");
  const auto cameraBufferId = glRenderer->CreateBuffer("CameraBuffer", sizeof(CameraTransform));
  const auto materialBuffer = glRenderer->GetBuffer(materialBufferId);
  const auto transformBuffer = glRenderer->GetBuffer(transformBufferId);
  const auto cameraBuffer = glRenderer->GetBuffer(cameraBufferId);
  if (!materialBuffer || !transformBuffer || !cameraBuffer)
    return;
  shader->Use();
  shader->SetCamera(*camera, cameraBuffer->id);
  const GLSkybox *skybox{nullptr};
  scene->ForFirstEntity<GLSkybox>([&](const EntityID, const GLSkybox *skyboxComp) {
    skybox = skyboxComp;
  });
  shader->SetSkybox(skybox);
  std::vector<Light> lights;
  scene->ForEachEntity<Light>([&](EntityID, const Light *light) {
    lights.push_back(*light);
  });
  shader->SetLighting(lights);
  shader->SetMaterial(material);
  shader->SetMaterialFallback(mesh, fallbacks, materialBuffer->id);
  shader->SetTransform(mesh, transforms, transformBuffer->id);
  shader->Draw(mesh, transforms.size());
}
auto RenderingSystem::DrawSkybox(Renderer &renderer) -> void {
  auto glRenderer = renderer.As<GLRenderer>();
  if (!glRenderer)
    return;
  auto scene = glRenderer->GetScene();
  if (!scene)
    return;
  auto camera = scene->GetActiveCamera();
  if (!camera)
    return;
  auto shader = glRenderer->GetShader("Skybox");
  if (!shader)
    return;
  const auto mesh = glRenderer->GetPrimitive("CubeInverted");
  if (!mesh)
    return;
  const auto cameraBufferId = glRenderer->CreateBuffer("CameraBuffer", sizeof(CameraTransform));
  const auto cameraBuffer = glRenderer->GetBuffer(cameraBufferId);
  if (!cameraBuffer)
    return;
  shader->Use();
  shader->SetCamera(*camera, cameraBuffer->id);
  const auto model = glm::scale(glm::mat4(1.f), glm::vec3(2.f));
  shader->SetUniform("model", model);
  GLSkybox *skybox{};
  scene->ForFirstEntity<GLSkybox>([&skybox](const EntityID id, GLSkybox *skyboxComp) {
    skybox = skyboxComp;
  });
  if (!skybox || skybox->skybox == 0)
    shader->SetUniform("useSkybox", false);
  else {
    shader->SetTexture("skybox", skybox->skybox);
    shader->SetUniform("useSkybox", true);
  }
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);
  shader->Draw(*mesh);
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);
}
} // namespace kuki
