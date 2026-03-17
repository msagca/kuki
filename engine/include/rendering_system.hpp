#pragma once
#include <asset_manager.hpp>
#include <gl_renderer.hpp>
#include <kuki_engine_export.h>
#include <material_type.hpp>
#include <memory>
#include <render_graph.hpp>
#include <render_graph_builder.hpp>
#include <scene_manager.hpp>
#include <shader_asset.hpp>
#include <system.hpp>
namespace kuki {
class KUKI_ENGINE_API RenderingSystem final : public System {
public:
  RenderingSystem(SceneManager &, AssetManager &);
  ~RenderingSystem();
  auto Awake() -> void override;
  auto Start() -> void override;
  auto Update(const float) -> void override;
  auto Shutdown() -> void override;
  auto GetFinalTarget() -> RenderTarget *;
  auto GetFPS() const -> size_t;
  auto GetTarget(const std::string &) -> RenderTarget *;
  auto LoadCompute(ShaderAsset &) -> void;
  auto LoadPrimitive(const std::string &) -> void;
  auto LoadShader(ShaderAsset &, ShaderAsset &) -> void;
private:
  Renderer *activeRenderer;
  GLRenderer glRenderer;
  RenderGraphBuilder graphBuilder;
  SceneManager &sceneManager;
  size_t fps{};
  std::unique_ptr<RenderGraph> renderGraph;
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
  static auto DrawEntities(Renderer &) -> void;
  static auto DrawEntitiesInstanced(Renderer &, const GLMesh &, const GLMaterial &, const std::vector<MaterialFallback> &, const std::vector<glm::mat4> &) -> void;
  static auto DrawSkybox(Renderer &) -> void;
};
} // namespace kuki
