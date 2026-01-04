#pragma once
#include <gl_render_target.hpp>
#include <gl_resource_manager.hpp>
#include <gl_skybox.hpp>
#include <render_graph.hpp>
#include <renderer.hpp>
#include <target_description.hpp>
namespace kuki {
class KUKI_ENGINE_API GLRenderer final : public Renderer {
public:
  using Renderer::Renderer;
  auto BorrowBuffer(const BufferDescription &) -> unsigned int;
  auto BorrowFramebuffer() -> unsigned int;
  auto BorrowRenderbuffer(const TargetDescription &) -> unsigned int;
  auto BorrowTexture(const TargetDescription &) -> unsigned int;
  auto CreateBuffer(std::string, const BufferDescription &) -> BufferObject * override;
  auto CreateCompute(const ComputeType, const ShaderAsset &) -> GLComputeShader * override;
  auto CreatePrimitive(const PrimitiveType) -> BufferObject * override;
  auto CreateShader(const MaterialType, const ShaderAsset &, const ShaderAsset &) -> GLShader * override;
  auto CreateTarget(std::string, const TargetDescription &) -> GLRenderTarget * override;
  auto GetActiveScene() -> Scene * override;
  auto GetCompute(const ComputeType) -> GLComputeShader * override;
  auto GetPrimitive(const PrimitiveType) -> GLMesh * override;
  auto GetScene(const std::string &) -> Scene * override;
  auto GetShader(const MaterialType) -> GLShader * override;
  auto GetTarget(const std::string &) -> GLRenderTarget * override;
  auto Reset() -> void override;
  static auto ApplyAntiAliasing(Renderer &, std::span<std::string>, std::span<TargetBinding>) -> void;
  static auto ApplyBloomEffect(Renderer &, std::span<std::string>, std::span<TargetBinding>) -> void;
  static auto ApplyBlurEffect(Renderer &, std::span<std::string>, std::span<TargetBinding>) -> void;
  static auto ApplyBrightPassFilter(Renderer &, std::span<std::string>, std::span<TargetBinding>) -> void;
  static auto ApplyGammaCorrection(Renderer &, std::span<std::string>, std::span<TargetBinding>) -> void;
  static auto ConvertCubemapToEquirectangularMap(Renderer &, const std::string &, const TargetBinding &) -> void;
  static auto ConvertEquirectangularMapToCubemap(Renderer &, const std::string &, const TargetBinding &) -> void;
  static auto CreateBRDF_LUT(Renderer &, const TargetBinding &) -> void;
  static auto CreateIrradianceMap(Renderer &, const std::string &, const TargetBinding &) -> void;
  static auto CreatePrefilterMap(Renderer &, const std::string &, const TargetBinding &) -> void;
  static auto RenderScene(Renderer &, std::span<std::string>, std::span<TargetBinding>) -> void;
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
  static auto DrawEntities(Renderer &) -> void;
  static auto DrawEntitiesInstanced(Renderer &, const GLMesh &, const GLMaterial &, const std::vector<MaterialFallback> &, const std::vector<glm::mat4> &) -> void;
  static auto DrawSkybox(Renderer &) -> void;
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
