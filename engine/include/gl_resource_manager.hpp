#pragma once
#include <buffer_description.hpp>
#include <buffer_pool.hpp>
#include <component_manager.hpp>
#include <compute_type.hpp>
#include <entity_manager.hpp>
#include <framebuffer_pool.hpp>
#include <gl_buffer.hpp>
#include <gl_compute_shader.hpp>
#include <gl_framebuffer.hpp>
#include <gl_mesh.hpp>
#include <gl_render_target.hpp>
#include <gl_renderbuffer.hpp>
#include <gl_shader.hpp>
#include <gl_texture.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <material_type.hpp>
#include <mesh_asset.hpp>
#include <primitive_type.hpp>
#include <renderbuffer_pool.hpp>
#include <target_description.hpp>
#include <texture_asset.hpp>
#include <texture_pool.hpp>
#include <unordered_map>
namespace kuki {
class KUKI_ENGINE_API GLResourceManager {
public:
  auto BorrowBuffer(const BufferDescription &) -> unsigned int;
  auto BorrowFramebuffer() -> unsigned int;
  auto BorrowRenderbuffer(const TargetDescription &) -> unsigned int;
  auto BorrowTexture(const TargetDescription &) -> unsigned int;
  auto Clear() -> void;
  auto CreateBuffer(std::string, const BufferDescription &) -> EntityID;
  auto CreateCompute(const ComputeType, const ShaderAsset &) -> EntityID;
  auto CreateMesh(std::string, const MeshAsset &) -> EntityID;
  auto CreatePrimitive(const PrimitiveType) -> EntityID;
  auto CreateShader(const MaterialType, const ShaderAsset &, const ShaderAsset &) -> EntityID;
  auto CreateTarget(std::string, const TargetDescription &) -> EntityID;
  auto CreateTexture(std::string, const TextureAsset &) -> EntityID;
  auto GetBuffer(const EntityID) -> GLBuffer *;
  auto GetBuffer(const std::string &) -> GLBuffer *;
  auto GetCompute(const ComputeType) -> GLComputeShader *;
  auto GetID(const std::string &) -> EntityID;
  auto GetMesh(const EntityID) -> GLMesh *;
  auto GetMesh(const std::string &) -> GLMesh *;
  auto GetPrimitive(const PrimitiveType) -> GLMesh *;
  auto GetShader(const MaterialType) -> GLShader *;
  auto GetTarget(const EntityID) -> GLRenderTarget *;
  auto GetTarget(const std::string &) -> GLRenderTarget *;
  auto GetTexture(const EntityID) -> GLTexture *;
  auto GetTexture(const std::string &) -> GLTexture *;
  static auto TargetFormatToGL(const TargetFormat &) -> unsigned int;
  static auto TargetTypeToGL(const TargetType &) -> unsigned int;
  template <AreUnsignedInt... Vals>
  auto ReturnBuffer(const BufferDescription &, Vals &&...) -> void;
  template <AreUnsignedInt... Vals>
  auto ReturnFramebuffer(Vals &&...) -> void;
  template <AreUnsignedInt... Vals>
  auto ReturnRenderbuffer(const TargetDescription &, Vals &&...) -> void;
  template <AreUnsignedInt... Vals>
  auto ReturnTexture(const TargetDescription &, Vals &&...) -> void;
private:
  /// @brief Used to store GPU resource handles (IDs) in the form of OpenGL specific components (e.g., `GLMesh`)
  EntityManager entityManager;
  BufferPool bufferPool;
  FramebufferPool framebufferPool;
  RenderbufferPool renderbufferPool;
  TexturePool texturePool;
  std::unordered_map<AssetID, EntityID> assetToEntityId;
  std::unordered_map<ComputeType, EntityID> computeToEntityId;
  std::unordered_map<MaterialType, EntityID> shaderToEntityId;
  std::unordered_map<PrimitiveType, AssetID> primitiveToAssetId;
  auto CalculateBounds(GLMesh &, const std::vector<Vertex> &) -> void;
  auto CreateIndexBuffer(GLMesh &, const std::vector<unsigned int> &) -> void;
  auto CreateVertexBuffer(GLMesh &, const std::vector<Vertex> &, bool = false) -> void;
};
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
} // namespace kuki
