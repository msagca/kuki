#pragma once
#include <application.hpp>
#include <array>
#include <asset_manager.hpp>
#include <kuki_engine_export.h>
#include <material_type.hpp>
#include <memory>
#include <render_graph.hpp>
#include <render_graph_builder.hpp>
#include <renderer.hpp>
#include <scene_manager.hpp>
#include <shader_asset.hpp>
#include <system.hpp>
#include <target_description.hpp>
namespace kuki {
enum class RenderingAPI : uint8_t {
  DirectX,
  OpenGL,
  Vulkan
};
class KUKI_ENGINE_API RenderingSystem final : public System {
public:
  RenderingSystem(Application &);
  auto Start() -> void override;
  auto Update(const float) -> void override;
  auto Shutdown() -> void override;
  auto GetFPS() const -> size_t;
  auto GetPreviewSize() const -> int;
  auto GetResolution() const -> std::pair<int, int>;
  auto GetTarget(std::string = "") -> RenderTarget *;
  auto ForEachTarget(auto &&) const -> void;
  auto LoadAssets(const AssetType) -> void;
  auto PickEntity(const int, const int) -> EntityID;
  auto PreviewAsset(const AssetID) -> RenderTarget *;
  auto SetPreviewSize(const int) -> void;
  auto SetRenderer(const RenderingAPI = RenderingAPI::OpenGL) -> void;
  auto SetResolution(const int = 1920, const int = 1080) -> void;
private:
  RenderGraphBuilder graphBuilder;
  std::unique_ptr<RenderGraph> renderGraph;
  Renderer *activeRenderer{};
  std::array<std::unique_ptr<Renderer>, static_cast<uint8_t>(RenderingAPI::Vulkan) + 1> renderers;
  size_t fps{};
  int screenWidth{1920};
  int screenHeight{1080};
};
auto RenderingSystem::ForEachTarget(auto &&func) const -> void {
  if (renderGraph)
    renderGraph->ForEachTarget(std::forward<decltype(func)>(func));
}
} // namespace kuki
