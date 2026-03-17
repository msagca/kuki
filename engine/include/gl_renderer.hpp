#pragma once
#include <asset_manager.hpp>
#include <gl_render_target.hpp>
#include <gl_resource_manager.hpp>
#include <gl_skybox.hpp>
#include <render_graph.hpp>
#include <renderer.hpp>
#include <scene_manager.hpp>
#include <target_description.hpp>
namespace kuki {
class KUKI_ENGINE_API GLRenderer final : public Renderer {
public:
  GLRenderer(SceneManager &, AssetManager &);
  auto BorrowBuffer(const BufferDescription &) -> unsigned int;
  auto BorrowFramebuffer() -> unsigned int;
  auto BorrowRenderbuffer(const TargetDescription &) -> unsigned int;
  auto BorrowTexture(const TargetDescription &) -> unsigned int;
  auto CreateBuffer(std::string, const BufferDescription &) -> GLBuffer * override;
  auto CreateTarget(std::string, const TargetDescription &) -> GLRenderTarget * override;
  auto GetActiveScene() -> Scene * override;
  auto GetBuffer(const std::string &) -> GLBuffer * override;
  auto GetCompute(const std::string &) -> GLComputeShader * override;
  auto GetPrimitive(const std::string &) -> GLMesh * override;
  auto GetScene(const std::string &) -> Scene * override;
  auto GetShader(const MaterialType) -> GLShader * override;
  auto GetShader(const std::string &, const MaterialType = MaterialType::Unknown) -> GLShader * override;
  auto GetTarget(const std::string &) -> GLRenderTarget * override;
  auto GetTargetCopy(const std::string &) -> GLRenderTarget;
  auto LoadCompute(ShaderAsset &) -> GLComputeShader * override;
  auto LoadPrimitive(const std::string &) -> GLMesh * override;
  auto LoadShader(ShaderAsset &, ShaderAsset &) -> GLShader * override;
  auto PrepareScene(Scene &) -> void override;
  auto Clear() -> void override;
  auto Reset() -> void override;
  template <AreUnsignedInt... Vals>
  auto ReturnBuffer(const BufferDescription &, Vals &&...) -> void;
  template <AreUnsignedInt... Vals>
  auto ReturnFramebuffer(Vals &&...) -> void;
  template <AreUnsignedInt... Vals>
  auto ReturnRenderbuffer(const TargetDescription &, Vals &&...) -> void;
  template <AreUnsignedInt... Vals>
  auto ReturnTexture(const TargetDescription &, Vals &&...) -> void;
private:
  GLResourceManager resourceManager;
};
template <AreUnsignedInt... Vals>
auto GLRenderer::ReturnBuffer(const BufferDescription &desc, Vals &&...vals) -> void {
  resourceManager.ReturnBuffer(vals...);
}
template <AreUnsignedInt... Vals>
auto GLRenderer::ReturnFramebuffer(Vals &&...vals) -> void {
  resourceManager.ReturnFramebuffer(vals...);
}
template <AreUnsignedInt... Vals>
auto GLRenderer::ReturnRenderbuffer(const TargetDescription &desc, Vals &&...vals) -> void {
  resourceManager.ReturnRenderbuffer(desc, vals...);
}
template <AreUnsignedInt... Vals>
auto GLRenderer::ReturnTexture(const TargetDescription &desc, Vals &&...vals) -> void {
  resourceManager.ReturnTexture(desc, vals...);
}
} // namespace kuki
