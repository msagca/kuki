#pragma once
#include <asset_manager.hpp>
#include <buffer_description.hpp>
#include <buffer_pool.hpp>
#include <component_manager.hpp>
#include <concepts.hpp>
#include <entity_manager.hpp>
#include <framebuffer_pool.hpp>
#include <gl_buffer.hpp>
#include <gl_compute_shader.hpp>
#include <gl_framebuffer.hpp>
#include <gl_lit_shader.hpp>
#include <gl_mesh.hpp>
#include <gl_render_target.hpp>
#include <gl_renderbuffer.hpp>
#include <gl_shader.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <gl_unlit_shader.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <render_graph.hpp>
#include <renderbuffer_pool.hpp>
#include <renderer.hpp>
#include <scene_asset.hpp>
#include <scene_manager.hpp>
#include <skybox_asset.hpp>
#include <target_description.hpp>
#include <texture_pool.hpp>
//
#include <glad/glad.h>
namespace kuki {
class KUKI_ENGINE_API GLRenderer final : public Renderer {
public:
  GLRenderer(SceneManager &, AssetManager &);
  static auto GLFormatToTarget(const unsigned int) -> TargetFormat;
  static auto GLTypeToTarget(const unsigned int) -> TargetType;
  static auto TargetFormatToGL(const TargetFormat &) -> unsigned int;
  static auto TargetTypeToGL(const TargetType &) -> unsigned int;
  auto BorrowBuffer(const BufferDescription &) -> unsigned int;
  auto BorrowFramebuffer() -> unsigned int;
  auto BorrowRenderbuffer(const TargetDescription &) -> unsigned int;
  auto BorrowTexture(const TargetDescription &) -> unsigned int;
  auto Clear() -> void override;
  auto CreateBuffer(std::string, const BufferDescription &) -> GLBuffer * override;
  auto CreateTarget(std::string, const TargetDescription &) -> GLRenderTarget * override;
  auto CreateTexture(std::string, const TargetDescription &) -> GLTexture * override;
  auto GetBuffer(const std::string &) -> GLBuffer * override;
  auto GetCompute(const std::string &) -> GLComputeShader * override;
  auto GetPrimitive(const std::string &) -> GLMesh * override;
  auto GetResourceID(const std::string &) const -> EntityID;
  auto GetScene(const std::string & = "") -> Scene * override;
  auto GetShader(const MaterialType) -> GLShader * override;
  auto GetShader(const std::string &, const MaterialType = MaterialType::Unknown) -> GLShader * override;
  auto GetTarget(const std::string &) -> GLRenderTarget * override;
  auto GetTargetCopy(const std::string &) -> GLRenderTarget;
  auto GetTexture(const std::string &) -> GLTexture * override;
  auto LoadCompute(ShaderAsset &) -> GLComputeShader * override;
  auto LoadPrimitive(const std::string &) -> GLMesh * override;
  auto LoadShader(ShaderAsset &, ShaderAsset &) -> GLShader * override;
  auto Reset() -> void override;
  auto UpdateScene(Scene &) -> void override;
  template <typename T>
  auto GetComponent(this auto &, const EntityID) -> decltype(auto);
  template <typename T>
  auto GetComponent(this auto &, const std::string & = "") -> decltype(auto);
  template <IsAsset T>
  auto LoadAsset(T &) -> void;
  template <IsAsset T>
  auto LoadAsset(T &, T &) -> void;
  template <AreUnsignedInt... Vals>
  auto ReturnBuffer(const BufferDescription &, Vals &&...) -> void;
  template <AreUnsignedInt... Vals>
  auto ReturnFramebuffer(Vals &&...) -> void;
  template <AreUnsignedInt... Vals>
  auto ReturnRenderbuffer(const TargetDescription &, Vals &&...) -> void;
  template <AreUnsignedInt... Vals>
  auto ReturnTexture(const TargetDescription &, Vals &&...) -> void;
private:
  AssetManager &assetManager;
  SceneManager &sceneManager;
  EntityManager resourceManager;
  BufferPool bufferPool;
  FramebufferPool framebufferPool;
  RenderbufferPool renderbufferPool;
  TexturePool texturePool;
  static auto CalculateBounds(GLMesh &, const std::vector<Vertex> &) -> void;
  static auto CreateIndexBuffer(GLMesh &, const std::vector<unsigned int> &) -> void;
  static auto CreateVertexBuffer(GLMesh &, const std::vector<Vertex> &, bool = false) -> void;
  auto LoadSceneMaterial(SceneAsset &, size_t) -> EntityID;
  auto LoadSceneMesh(SceneAsset &, const size_t) -> EntityID;
  auto LoadSceneTexture(SceneAsset &, size_t) -> GLTexture *;
};
template <typename T>
auto GLRenderer::GetComponent(this auto &self, const EntityID id) -> decltype(auto) {
  return self.resourceManager.template GetComponent<T>(id);
}
template <typename T>
auto GLRenderer::GetComponent(this auto &self, const std::string &name) -> decltype(auto) {
  if (name.empty())
    return self.resourceManager.template GetAny<T>();
  return self.resourceManager.template GetComponent<T>(name);
}
template <IsAsset T>
auto GLRenderer::LoadAsset(T &) -> void {}
template <IsAsset T>
auto GLRenderer::LoadAsset(T &, T &) -> void {}
template <AreUnsignedInt... Vals>
auto GLRenderer::ReturnBuffer(const BufferDescription &desc, Vals &&...vals) -> void {
  bufferPool.Release(desc, vals...);
}
template <AreUnsignedInt... Vals>
auto GLRenderer::ReturnFramebuffer(Vals &&...vals) -> void {
  framebufferPool.Release(vals...);
}
template <AreUnsignedInt... Vals>
auto GLRenderer::ReturnRenderbuffer(const TargetDescription &desc, Vals &&...vals) -> void {
  renderbufferPool.Release(desc, vals...);
}
template <AreUnsignedInt... Vals>
auto GLRenderer::ReturnTexture(const TargetDescription &desc, Vals &&...vals) -> void {
  texturePool.Release(desc, vals...);
}
#include <gl_renderer.inl>
} // namespace kuki
