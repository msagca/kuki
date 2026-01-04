#pragma once
#include <compute_type.hpp>
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
  RenderingSystem(SceneManager &);
  ~RenderingSystem();
  auto LateUpdate(float) -> void override;
  auto Shutdown() -> void override;
  auto Start() -> void override;
  auto Update(float) -> void override;
  auto ActivateScene(const Scene &) -> void;
  auto DeactivateScene(const Scene &) -> void;
  auto GetFPS() const -> size_t;
  auto GetTarget(const std::string &) -> RenderTarget *;
  auto LoadCompute(const ComputeType, const ShaderAsset &) -> void;
  auto LoadPrimitive(const PrimitiveType) -> void;
  auto LoadScene(const Scene &) -> void;
  auto LoadShader(const MaterialType, const ShaderAsset &, const ShaderAsset &) -> void;
  auto UnloadScene(const Scene &) -> void;
private:
  GLRenderer glRenderer;
  RenderGraphBuilder graphBuilder;
  SceneManager &sceneManager;
  size_t fps{};
  std::unique_ptr<RenderGraph> renderGraph;
};
} // namespace kuki
