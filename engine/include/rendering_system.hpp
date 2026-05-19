#pragma once
#include <application_settings.hpp>
#include <asset_manager.hpp>
#include <gl_renderer.hpp>
#include <kuki_engine_export.h>
#include <material_type.hpp>
#include <memory>
#include <render_graph.hpp>
#include <render_graph_builder.hpp>
#include <scene_manager.hpp>
#include <settings_manager.hpp>
#include <shader_asset.hpp>
#include <system.hpp>
#include <target_description.hpp>
namespace kuki {
class KUKI_ENGINE_API RenderingSystem final : public System {
public:
  RenderingSystem(SceneManager &, AssetManager &, SettingsManager &);
  ~RenderingSystem();
  auto Start() -> void override;
  auto Update(const float) -> void override;
  auto Shutdown() -> void override;
  auto GetFPS() const -> size_t;
  auto GetTarget(std::string = "") -> RenderTarget *;
  auto LoadAsset(const AssetID) -> void;
  auto PreviewAsset(const AssetID, int = 128) -> RenderTarget *;
private:
  AssetManager &assetManager;
  SceneManager &sceneManager;
  SettingsManager &settingsManager;
  Renderer *activeRenderer{};
  GLRenderer glRenderer;
  RenderGraphBuilder graphBuilder;
  std::unique_ptr<RenderGraph> renderGraph;
  size_t fps{};
  auto OnResolutionChanged(const ScreenResolution &) -> void;
  auto OnSceneLoaded(Scene &) -> void;
  static auto ApplyAntiAliasing(Renderer &, std::span<std::string>, std::span<std::string>) -> void;
  static auto ApplyBloomEffect(Renderer &, std::span<std::string>, std::span<std::string>) -> void;
  static auto ApplyBlurEffect(Renderer &, std::span<std::string>, std::span<std::string>) -> void;
  static auto ApplyBrightPassFilter(Renderer &, std::span<std::string>, std::span<std::string>) -> void;
  static auto ApplyGammaCorrection(Renderer &, std::span<std::string>, std::span<std::string>) -> void;
  static auto CreateShadowMap(Renderer &, std::span<std::string>, std::span<std::string>) -> void;
  static auto RenderScene(Renderer &, std::span<std::string>, std::span<std::string>) -> void;
  static auto DrawEntities(Renderer &, std::span<std::string>) -> void;
  static auto DrawEntitiesInstanced(Renderer &, std::span<std::string>, const GLMesh &, const GLMaterial &, const std::vector<MaterialFallback> &, const std::vector<glm::mat4> &) -> void;
  static auto DrawMeshes(Renderer &) -> void;
  static auto DrawMeshesInstanced(Renderer &, const GLMesh &, const std::vector<glm::mat4> &) -> void;
  static auto DrawSkybox(Renderer &) -> void;
};
} // namespace kuki
