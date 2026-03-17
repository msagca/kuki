#include <asset_manager.hpp>
#include <buffer_object.hpp>
#include <gl_buffer.hpp>
#include <gl_compute_shader.hpp>
#include <gl_mesh_material.hpp>
#include <gl_render_target.hpp>
#include <gl_renderer.hpp>
#include <gl_resource_manager.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <id.hpp>
#include <material_handle.hpp>
#include <material_type.hpp>
#include <mesh_handle.hpp>
#include <primitive.hpp>
#include <render_graph.hpp>
#include <render_target.hpp>
#include <renderer.hpp>
#include <scene.hpp>
#include <skybox_handle.hpp>
#include <target_description.hpp>
//
#include <glad/glad.h>
namespace kuki {
GLRenderer::GLRenderer(SceneManager &sceneManager, AssetManager &assetManager)
  : Renderer(sceneManager), resourceManager(assetManager) {}
auto GLRenderer::BorrowBuffer(const BufferDescription &desc) -> unsigned int {
  return resourceManager.BorrowBuffer(desc);
}
auto GLRenderer::BorrowFramebuffer() -> unsigned int {
  return resourceManager.BorrowFramebuffer();
}
auto GLRenderer::BorrowRenderbuffer(const TargetDescription &desc) -> unsigned int {
  return resourceManager.BorrowRenderbuffer(desc);
}
auto GLRenderer::BorrowTexture(const TargetDescription &desc) -> unsigned int {
  return resourceManager.BorrowTexture(desc);
}
auto GLRenderer::CreateBuffer(std::string name, const BufferDescription &desc) -> GLBuffer * {
  const auto &id = resourceManager.CreateBuffer(name, desc);
  return GetBuffer(name);
}
auto GLRenderer::CreateTarget(std::string name, const TargetDescription &desc) -> GLRenderTarget * {
  const auto &id = resourceManager.CreateTarget(name, desc);
  return GetTarget(name);
}
auto GLRenderer::GetActiveScene() -> Scene * {
  return sceneManager.GetActive();
}
auto GLRenderer::GetBuffer(const std::string &name) -> GLBuffer * {
  return resourceManager.GetComponent<GLBuffer>(name);
}
auto GLRenderer::GetCompute(const std::string &name) -> GLComputeShader * {
  return resourceManager.GetComponent<GLComputeShader>(name);
}
auto GLRenderer::GetPrimitive(const std::string &name) -> GLMesh * {
  return resourceManager.GetComponent<GLMesh>(name);
}
auto GLRenderer::GetScene(const std::string &name) -> Scene * {
  return sceneManager.Get(name);
}
auto GLRenderer::GetShader(const MaterialType type) -> GLShader * {
  switch (type) {
  case MaterialType::Lit:
  case MaterialType::LitSkinned:
    return resourceManager.GetComponent<GLLitShader>();
  case MaterialType::Unlit:
    return resourceManager.GetComponent<GLUnlitShader>();
  default:
    return resourceManager.GetComponent<GLShader>();
  }
}
auto GLRenderer::GetShader(const std::string &name, const MaterialType type) -> GLShader * {
  switch (type) {
  case MaterialType::Lit:
  case MaterialType::LitSkinned:
    return resourceManager.GetComponent<GLLitShader>(name);
  case MaterialType::Unlit:
    return resourceManager.GetComponent<GLUnlitShader>(name);
  default:
    return resourceManager.GetComponent<GLShader>(name);
  }
}
auto GLRenderer::GetTarget(const std::string &name) -> GLRenderTarget * {
  return resourceManager.GetComponent<GLRenderTarget>(name);
}
auto GLRenderer::GetTargetCopy(const std::string &name) -> GLRenderTarget {
  auto target = resourceManager.GetComponent<GLRenderTarget>(name);
  if (target)
    return *target;
  return {};
}
auto GLRenderer::LoadCompute(ShaderAsset &compute) -> GLComputeShader * {
  resourceManager.LoadAsset<ShaderAsset>(compute);
  return resourceManager.GetComponent<GLComputeShader>(compute.resourceId);
}
auto GLRenderer::LoadPrimitive(const std::string &name) -> GLMesh * {
  const auto &id = resourceManager.LoadPrimitive(name);
  return resourceManager.GetComponent<GLMesh>(id);
}
auto GLRenderer::LoadShader(ShaderAsset &vert, ShaderAsset &frag) -> GLShader * {
  resourceManager.LoadAsset<ShaderAsset>(vert, frag);
  return resourceManager.GetComponent<GLShader>(frag.resourceId);
}
auto GLRenderer::PrepareScene(Scene &scene) -> void {
  resourceManager.PrepareScene(scene);
}
auto GLRenderer::Clear() -> void {
  resourceManager.Clear();
}
auto GLRenderer::Reset() -> void {
  glClearColor(0.f, 0.f, 0.f, 0.f);
}
} // namespace kuki
