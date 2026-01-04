#define GLM_ENABLE_EXPERIMENTAL
#include <application.hpp>
#include <archetype.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <entity_manager.hpp>
#include <enum_traits.hpp>
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
#include <primitive_type.hpp>
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
RenderingSystem::RenderingSystem(SceneManager &sceneManager)
  : System(std::in_place_type<RenderingSystem>), sceneManager(sceneManager), glRenderer(sceneManager) {}
RenderingSystem::~RenderingSystem() {
  Shutdown();
}
auto RenderingSystem::LateUpdate(float deltaTime) -> void {
}
auto RenderingSystem::Shutdown() -> void {
}
auto RenderingSystem::Start() -> void {
  LoadPrimitive(PrimitiveType::Cube);
  LoadPrimitive(PrimitiveType::CubeInverted);
  LoadPrimitive(PrimitiveType::Cylinder);
  LoadPrimitive(PrimitiveType::Frame);
  LoadPrimitive(PrimitiveType::Plane);
  LoadPrimitive(PrimitiveType::Sphere);
  const auto desc = TargetDescription{.width = 1920, .height = 1080};
  renderGraph = graphBuilder
                  .BeginGraph()
                  .BeginPass(glRenderer.RenderScene)
                  .AddInput("Scene")
                  .AddOutput("SceneLinear", desc)
                  .EndPass()
                  .BeginPass(glRenderer.ApplyAntiAliasing)
                  .AddInput("SceneLinear")
                  .AddOutput("SceneMSAA", desc)
                  .EndPass()
                  .BeginPass(glRenderer.ApplyBlurEffect)
                  .AddInput("SceneMSAA")
                  .AddOutput("SceneBlur", desc)
                  .EndPass()
                  .BeginPass(glRenderer.ApplyBrightPassFilter)
                  .AddInput("SceneBlur")
                  .AddOutput("SceneBright", desc)
                  .EndPass()
                  .BeginPass(glRenderer.ApplyBloomEffect)
                  .AddInput("SceneLinear")
                  .AddInput("SceneBright")
                  .AddOutput("SceneBloom", desc)
                  .EndPass()
                  .BeginPass(glRenderer.ApplyGammaCorrection)
                  .AddInput("SceneBloom")
                  .AddOutput("SceneSRGB", desc)
                  .EndPass()
                  .EndGraph();
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
  renderGraph->Execute(glRenderer);
}
auto RenderingSystem::ActivateScene(const Scene &scene) -> void {
  scene.ForEachEntity([this](const EntityID id) {});
}
auto RenderingSystem::DeactivateScene(const Scene &scene) -> void {
  scene.ForEachEntity([this](const EntityID id) {});
}
auto RenderingSystem::GetFPS() const -> size_t {
  return fps;
}
auto RenderingSystem::GetTarget(const std::string &name) -> RenderTarget * {
  return glRenderer.GetTarget(name);
}
auto RenderingSystem::LoadCompute(const ComputeType type, const ShaderAsset &comp) -> void {
  glRenderer.CreateCompute(type, comp);
}
auto RenderingSystem::LoadPrimitive(const PrimitiveType type) -> void {
  glRenderer.CreatePrimitive(type);
}
auto RenderingSystem::LoadScene(const Scene &scene) -> void {
  scene.ForEachEntity([this](const EntityID id) {});
}
auto RenderingSystem::LoadShader(const MaterialType type, const ShaderAsset &vert, const ShaderAsset &frag) -> void {
  glRenderer.CreateShader(type, vert, frag);
}
auto RenderingSystem::UnloadScene(const Scene &scene) -> void {
  scene.ForEachEntity([this](const EntityID id) {});
}
} // namespace kuki
