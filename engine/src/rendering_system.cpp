#define GLM_ENABLE_EXPERIMENTAL
#include <application.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <entity_manager.hpp>
#include <enum_traits.hpp>
#include <gl_mesh_material.hpp>
#include <gl_shader.hpp>
#include <glm/detail/type_vec3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <id.hpp>
#include <light.hpp>
#include <pool.hpp>
#include <primitive.hpp>
#include <renderer.hpp>
#include <rendering_system.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <system.hpp>
#include <target_description.hpp>
#include <texture_pool.hpp>
#include <transform.hpp>
//
#include <glad/glad.h>
namespace kuki {
RenderingSystem::RenderingSystem(SceneManager &sceneManager, AssetManager &assetManager)
  : System(std::in_place_type<RenderingSystem>), glRenderer(sceneManager, assetManager) {}
RenderingSystem::~RenderingSystem() {
  Shutdown();
}
auto RenderingSystem::Awake() -> void {
  activeRenderer = &glRenderer;
  LoadPrimitive("Cube");
  LoadPrimitive("CubeInverted");
  LoadPrimitive("Cylinder");
  LoadPrimitive("Frame");
  LoadPrimitive("Plane");
  LoadPrimitive("Sphere");
  // TODO: get the target dimensions from the application settings
  const auto desc = TargetDescription{.width = 1920, .height = 1080};
  renderGraph = graphBuilder
                  .BeginGraph()
                  .BeginPass(RenderScene)
                  .AddOutput("SceneLinear", desc)
                  .EndPass()
                  .BeginPass(ApplyAntiAliasing)
                  .AddInput("SceneLinear")
                  .AddOutput("SceneMSAA", desc)
                  .EndPass()
                  .BeginPass(ApplyBlurEffect)
                  .AddInput("SceneMSAA")
                  .AddOutput("SceneBlur", desc)
                  .EndPass()
                  .BeginPass(ApplyBrightPassFilter)
                  .AddInput("SceneBlur")
                  .AddOutput("SceneBright", desc)
                  .EndPass()
                  .BeginPass(ApplyBloomEffect)
                  .AddInput("SceneLinear")
                  .AddInput("SceneBright")
                  .AddOutput("SceneBloom", desc)
                  .EndPass()
                  .BeginPass(ApplyGammaCorrection)
                  .AddInput("SceneBloom")
                  .AddOutput("SceneSRGB", desc)
                  .EndPass()
                  .EndGraph();
  if (renderGraph)
    renderGraph->Compile();
}
auto RenderingSystem::Start() -> void {}
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
auto RenderingSystem::LoadCompute(ShaderAsset &comp) -> void {
  if (activeRenderer)
    activeRenderer->LoadCompute(comp);
}
auto RenderingSystem::LoadPrimitive(const std::string &name) -> void {
  if (activeRenderer)
    activeRenderer->LoadPrimitive(name);
}
auto RenderingSystem::LoadShader(ShaderAsset &vert, ShaderAsset &frag) -> void {
  if (activeRenderer)
    activeRenderer->LoadShader(vert, frag);
}
auto RenderingSystem::ApplyAntiAliasing(Renderer &renderer, std::span<std::string> inputs, std::span<TargetBinding> outputs) -> void {
  if (inputs.size() != 1 || outputs.size() != 1)
    return;
  auto glRenderer = renderer.As<GLRenderer>();
  if (!glRenderer)
    return;
  const auto &desc = outputs[0].desc;
  const auto in = glRenderer->GetTargetCopy(inputs[0]);
  const auto out = glRenderer->CreateTarget(outputs[0].name, outputs[0].desc);
  if (!out)
    return;
  glBindFramebuffer(GL_READ_FRAMEBUFFER, in.framebuffer);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, out->framebuffer);
  glBlitFramebuffer(0, 0, desc.width, desc.height, 0, 0, desc.width, desc.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto RenderingSystem::ApplyBloomEffect(Renderer &renderer, std::span<std::string> inputs, std::span<TargetBinding> outputs) -> void {
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
  const auto &desc = outputs[0].desc;
  const auto in0 = glRenderer->GetTargetCopy(inputs[0]);
  const auto in1 = glRenderer->GetTargetCopy(inputs[1]);
  const auto out = glRenderer->CreateTarget(outputs[0].name, outputs[0].desc);
  if (!out)
    return;
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glViewport(0, 0, desc.width, desc.height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  bloomShader->Use();
  bloomShader->SetTexture("image", in0.texture);
  bloomShader->SetTexture("imageBright", in1.texture);
  bloomShader->SetUniform("model", glm::mat4(1.f));
  bloomShader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto RenderingSystem::ApplyBlurEffect(Renderer &renderer, std::span<std::string> inputs, std::span<TargetBinding> outputs) -> void {
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
  const auto &desc = outputs[0].desc;
  const auto in = glRenderer->GetTargetCopy(inputs[0]);
  const auto out = glRenderer->CreateTarget(outputs[0].name, outputs[0].desc);
  if (!out)
    return;
  constexpr auto blurPasses = 8;
  blurShader->Use();
  blurShader->SetUniform("model", glm::mat4(1.f));
  for (auto i = 0; i < blurPasses; ++i) {
    const auto pingPong = i % 2 == 0; // NOTE: this is `true` in the first iteration, so `in->texture` is read first (as it should be)
    const auto srcImg = pingPong ? in.texture : out->texture;
    const auto dstBuf = pingPong ? out->framebuffer : in.framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, dstBuf);
    glViewport(0, 0, desc.width, desc.height);
    blurShader->SetTexture("image", srcImg);
    blurShader->SetUniform("horizontal", pingPong);
    blurShader->Draw(*mesh);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto RenderingSystem::ApplyBrightPassFilter(Renderer &renderer, std::span<std::string> inputs, std::span<TargetBinding> outputs) -> void {
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
  const auto &desc = outputs[0].desc;
  const auto in = glRenderer->GetTargetCopy(inputs[0]);
  const auto out = glRenderer->CreateTarget(outputs[0].name, outputs[0].desc);
  if (!out)
    return;
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glViewport(0, 0, desc.width, desc.height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  brightShader->Use();
  brightShader->SetTexture("image", in.texture);
  brightShader->SetUniform("model", glm::mat4(1.f));
  brightShader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto RenderingSystem::ApplyGammaCorrection(Renderer &renderer, std::span<std::string> inputs, std::span<TargetBinding> outputs) -> void {
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
  const auto &desc = outputs[0].desc;
  const auto in = glRenderer->GetTargetCopy(inputs[0]);
  const auto out = glRenderer->CreateTarget(outputs[0].name, outputs[0].desc);
  if (!out)
    return;
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glViewport(0, 0, desc.width, desc.height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  gammaShader->Use();
  gammaShader->SetTexture("image", in.texture);
  // TODO: do not hardcode these values
  gammaShader->SetUniform("exposure", 1.f);
  gammaShader->SetUniform("gamma", 2.2f);
  gammaShader->SetUniform("model", glm::mat4(1.f));
  gammaShader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto RenderingSystem::ConvertCubemapToEquirectangularMap(Renderer &renderer, const std::string &input, const TargetBinding &output) -> void {
  auto glRenderer = renderer.As<GLRenderer>();
  if (!glRenderer)
    return;
  auto compute = glRenderer->GetCompute("CubemapEquirect");
  if (!compute)
    return;
  const auto &desc = output.desc;
  const auto in = glRenderer->GetTargetCopy(input);
  const auto out = glRenderer->CreateTarget(output.name, output.desc);
  if (!out)
    return;
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  compute->SetTexture("cubemap", in.texture);
  compute->SetUniform("size", static_cast<unsigned int>(desc.width));
  const auto format = GLRenderer::TargetFormatToGL(desc.format);
  glBindImageTexture(0, out->texture, 0, GL_TRUE, 0, GL_WRITE_ONLY, format);
  const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
  const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
  compute->Dispatch(numGroupsX, numGroupsY, 6);
}
auto RenderingSystem::ConvertEquirectangularMapToCubemap(Renderer &renderer, const std::string &input, const TargetBinding &output) -> void {
  auto glRenderer = renderer.As<GLRenderer>();
  if (!glRenderer)
    return;
  auto compute = glRenderer->GetCompute("EquirectCubemap");
  if (!compute)
    return;
  const auto &desc = output.desc;
  const auto in = glRenderer->GetTargetCopy(input);
  const auto out = glRenderer->CreateTarget(output.name, output.desc);
  if (!out)
    return;
  const auto format = GLRenderer::TargetFormatToGL(desc.format);
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  compute->SetTexture("equirect", in.texture);
  compute->SetUniform("size", static_cast<unsigned int>(desc.width));
  // TODO: set the following to `true` if texture was loaded by TinyEXR
  compute->SetUniform("invert", false);
  glBindImageTexture(0, out->texture, 0, GL_TRUE, 0, GL_WRITE_ONLY, format);
  const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
  const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
  compute->Dispatch(numGroupsX, numGroupsY, 6);
}
auto RenderingSystem::CreateBRDF_LUT(Renderer &renderer, const TargetBinding &output) -> void {
  auto glRenderer = renderer.As<GLRenderer>();
  if (!glRenderer)
    return;
  auto compute = glRenderer->GetCompute("BRDF_LUT");
  if (!compute)
    return;
  const auto &desc = output.desc;
  const auto out = glRenderer->CreateTarget(output.name, output.desc);
  if (!out)
    return;
  const auto format = GLRenderer::TargetFormatToGL(desc.format);
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  glBindImageTexture(0, out->texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, format);
  const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
  const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
  compute->Dispatch(numGroupsX, numGroupsY, 1);
}
auto RenderingSystem::CreateIrradianceMap(Renderer &renderer, const std::string &input, const TargetBinding &output) -> void {
  auto glRenderer = renderer.As<GLRenderer>();
  if (!glRenderer)
    return;
  auto compute = glRenderer->GetCompute("IrradianceMap");
  if (!compute)
    return;
  const auto &desc = output.desc;
  const auto in = glRenderer->GetTargetCopy(input);
  const auto out = glRenderer->CreateTarget(output.name, output.desc);
  if (!out)
    return;
  const auto format = GLRenderer::TargetFormatToGL(desc.format);
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  compute->SetTexture("cubemap", in.texture);
  compute->SetUniform("cubeSize", static_cast<unsigned int>(desc.width));
  glBindImageTexture(0, out->texture, 0, GL_TRUE, 0, GL_WRITE_ONLY, format);
  const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
  const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
  compute->Dispatch(numGroupsX, numGroupsY, 6);
}
auto RenderingSystem::CreatePrefilterMap(Renderer &renderer, const std::string &input, const TargetBinding &output) -> void {
  auto glRenderer = renderer.As<GLRenderer>();
  if (!glRenderer)
    return;
  auto compute = glRenderer->GetCompute("PrefilterMap");
  if (!compute)
    return;
  const auto &desc = output.desc;
  const auto in = glRenderer->GetTargetCopy(input);
  const auto out = glRenderer->CreateTarget(output.name, output.desc);
  if (!out)
    return;
  const auto format = GLRenderer::TargetFormatToGL(desc.format);
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  compute->SetTexture("cubemap", in.texture);
  compute->SetTexture("mipLevels", desc.mipmaps);
  for (auto mip = 0; mip < desc.mipmaps; ++mip) {
    const auto mipSize = static_cast<unsigned int>(desc.width) >> mip;
    const auto roughness = static_cast<float>(mip) / (desc.mipmaps - 1);
    compute->SetUniform("roughness", roughness);
    compute->SetUniform("mipWidth", mipSize);
    compute->SetUniform("cubeSize", mipSize);
    glBindImageTexture(0, out->texture, mip, GL_TRUE, 0, GL_WRITE_ONLY, format);
    const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
    const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
    compute->Dispatch(numGroupsX, numGroupsY, 6);
  }
}
auto RenderingSystem::RenderScene(Renderer &renderer, std::span<std::string> inputs, std::span<TargetBinding> outputs) -> void {
  if (outputs.size() != 1)
    return;
  auto glRenderer = renderer.As<GLRenderer>();
  if (!glRenderer)
    return;
  auto scene = glRenderer->GetScene();
  if (!scene)
    return;
  const auto &desc = outputs[0].desc;
  const auto out = glRenderer->CreateTarget(outputs[0].name, outputs[0].desc);
  if (!out)
    return;
  glRenderer->UpdateScene(*scene); // TODO: optimize this
  glBindFramebuffer(GL_FRAMEBUFFER, out->framebuffer);
  glViewport(0, 0, desc.width, desc.height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  DrawSkybox(renderer);
  DrawEntities(renderer);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
  const auto bufMat = glRenderer->CreateBuffer("MaterialBuffer", {});
  const auto bufXfm = glRenderer->CreateBuffer("TransformBuffer", {});
  const auto bufDesc = BufferDescription{.size = sizeof(CameraTransform)};
  const auto camBuf = glRenderer->CreateBuffer("CameraBuffer", bufDesc);
  shader->Use();
  shader->SetCamera(*camera, camBuf->id);
  if (material.type == MaterialType::Lit) {
    const GLSkybox *skybox{};
    scene->ForFirstEntity<GLSkybox>([&](const EntityID, const GLSkybox *skyboxComp) {
      skybox = skyboxComp;
    });
    shader->SetSkybox(skybox);
    std::vector<Light> lights;
    scene->ForEachEntity<Light>([&](EntityID, const Light *light) {
      lights.push_back(*light);
    });
    shader->SetLighting(lights);
  }
  shader->SetMaterial(material);
  shader->SetMaterialFallback(mesh, fallbacks, bufMat->id);
  shader->SetTransform(mesh, transforms, bufXfm->id);
  shader->DrawInstanced(mesh, transforms.size());
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
  const auto bufDesc = BufferDescription{.size = sizeof(CameraTransform)};
  const auto camBuf = glRenderer->CreateBuffer("CameraBuffer", bufDesc);
  shader->Use();
  shader->SetCamera(*camera, camBuf->id);
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
