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
#include <gl_texture.hpp>
#include <gl_unlit_shader.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <renderbuffer_pool.hpp>
#include <scene_asset.hpp>
#include <skybox_asset.hpp>
#include <target_description.hpp>
#include <texture_pool.hpp>
//
#include <glad/glad.h>
namespace kuki {
class KUKI_ENGINE_API GLResourceManager {
public:
  GLResourceManager(AssetManager &);
  static auto GLFormatToTarget(const unsigned int) -> TargetFormat;
  static auto GLTypeToTarget(const unsigned int) -> TargetType;
  static auto TargetFormatToGL(const TargetFormat &) -> unsigned int;
  static auto TargetTypeToGL(const TargetType &) -> unsigned int;
  auto BorrowBuffer(const BufferDescription &) -> unsigned int;
  auto BorrowFramebuffer() -> unsigned int;
  auto BorrowRenderbuffer(const TargetDescription &) -> unsigned int;
  auto BorrowTexture(const TargetDescription &) -> unsigned int;
  auto Clear() -> void;
  auto CreateBuffer(std::string, const BufferDescription &) -> EntityID;
  auto CreateTarget(std::string, const TargetDescription &) -> EntityID;
  auto GetResourceID(const std::string &) const -> EntityID;
  auto LoadPrimitive(const std::string &) -> EntityID;
  /// @brief Replaces GPU resource handles with API-specific components
  auto PrepareScene(Scene &) -> void;
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
  BufferPool bufferPool;
  FramebufferPool framebufferPool;
  RenderbufferPool renderbufferPool;
  TexturePool texturePool;
  /// @brief Stores GPU resource handles (e.g., buffer IDs) as components (e.g., `GLMesh`)
  EntityManager resourceManager;
  // FIXME: `EntityManager` stores components by value, which results in slicing for derived types like `GLShader`
  AssetManager &assetManager;
  static auto CalculateBounds(GLMesh &, const std::vector<Vertex> &) -> void;
  static auto CreateIndexBuffer(GLMesh &, const std::vector<unsigned int> &) -> void;
  static auto CreateVertexBuffer(GLMesh &, const std::vector<Vertex> &, bool = false) -> void;
  auto LoadSceneMaterial(SceneAsset &, size_t) -> EntityID;
  auto LoadSceneMesh(SceneAsset &, const size_t) -> EntityID;
  auto LoadSceneTexture(SceneAsset &, size_t) -> GLTexture *;
};
template <typename T>
auto GLResourceManager::GetComponent(this auto &self, const EntityID id) -> decltype(auto) {
  return self.resourceManager.template GetComponent<T>(id);
}
template <typename T>
auto GLResourceManager::GetComponent(this auto &self, const std::string &name) -> decltype(auto) {
  if (name.empty())
    return self.resourceManager.template GetAny<T>();
  return self.resourceManager.template GetComponent<T>(name);
}
template <IsAsset T>
auto GLResourceManager::LoadAsset(T &) -> void {}
template <IsAsset T>
auto GLResourceManager::LoadAsset(T &, T &) -> void {}
template <AreUnsignedInt... Vals>
auto GLResourceManager::ReturnBuffer(const BufferDescription &desc, Vals &&...vals) -> void {
  bufferPool.Release(desc, vals...);
}
template <AreUnsignedInt... Vals>
auto GLResourceManager::ReturnFramebuffer(Vals &&...vals) -> void {
  framebufferPool.Release(vals...);
}
template <AreUnsignedInt... Vals>
auto GLResourceManager::ReturnRenderbuffer(const TargetDescription &desc, Vals &&...vals) -> void {
  renderbufferPool.Release(desc, vals...);
}
template <AreUnsignedInt... Vals>
auto GLResourceManager::ReturnTexture(const TargetDescription &desc, Vals &&...vals) -> void {
  texturePool.Release(desc, vals...);
}
template <>
inline auto GLResourceManager::LoadAsset<SkyboxAsset>(SkyboxAsset &skyboxAsset) -> void {
  if (!skyboxAsset.id || skyboxAsset.resourceId)
    return;
  const auto resourceId = resourceManager.Create(skyboxAsset.GetName());
  skyboxAsset.resourceId = resourceId;
  auto skybox = resourceManager.AddComponent<GLSkybox>(resourceId);
  // TODO: dispatch computes to create irradiance/prefilter maps
}
#include <gl_resource_manager.inl>
} // namespace kuki
