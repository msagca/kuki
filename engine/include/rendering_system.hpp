#pragma once
#include <application.hpp>
#include <array>
#include <asset_manager.hpp>
#include <debug_view.hpp>
#include <desktop_size.hpp>
#include <kuki_engine_export.h>
#include <indirect_lighting.hpp>
#include <material_type.hpp>
#include <memory>
#include <pool_policy.hpp>
#include <render_graph.hpp>
#include <render_pass.hpp>
#include <render_graph_builder.hpp>
#include <renderer.hpp>
#include <rendering_api.hpp>
#include <scene_manager.hpp>
#include <shader_asset.hpp>
#include <system.hpp>
#include <target_description.hpp>
#include <tone_mapper.hpp>
namespace kuki {
/// @brief Edge length of the directional shadow map, in texels.
///
/// Fixed rather than viewport-derived: shadow quality depends on how much world space each texel
/// covers, which has nothing to do with the size of the editor panel. 4096 gives a comfortable
/// margin for a single cascade; drop to 2048 if the 67 MB depth surface is not worth it.
inline constexpr int SHADOW_MAP_RESOLUTION = 4096;
/// @brief Edge length of each spot-light shadow map layer, in texels.
///
/// Multiplied by the spot shadow light limit, so it costs a layer per light.
inline constexpr int SPOT_SHADOW_MAP_RESOLUTION = 1024;
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
  /// @brief Which step of the indirect lighting the scene pass draws instead of the finished pixel.
  ///
  /// The render graph's targets cover the steps that end in a resource and can already be picked one
  /// by one. These are the ones that do not: values living inside the scene shader for the length of
  /// an expression, which nothing can see unless the shader is asked to write them out.
  auto GetLightingDebugView() const -> LightingDebugView;
  auto SetLightingDebugView(const LightingDebugView) -> void;
  /// @brief What the irradiance probes are drawn as, on a backend that has them.
  auto GetProbeDebugView() const -> ProbeDebugView;
  auto SetProbeDebugView(const ProbeDebugView) -> void;
  /// @brief Every value the indirect lighting is shaped by, as one group.
  ///
  /// Passed whole rather than one accessor per field. There are eighteen of them, they are only ever
  /// read together to fill a constant buffer, and the panel that edits them edits a copy and hands
  /// it back -- so a setter each would be eighteen ways to say the same thing.
  auto GetIndirectLighting() const -> const IndirectLighting &;
  auto SetIndirectLighting(const IndirectLighting &) -> void;
  /// @brief Whether a pass runs, or is stood down and its output made harmless.
  ///
  /// Standing a pass down is how the picture answers what that pass is worth: the occlusion, the
  /// bloom and the shadows are each easier to judge by their absence than by a description of them.
  /// See `Renderer::IsPassEnabled` for what "stood down" leaves behind, which is deliberately not
  /// the same as the pass being gone.
  auto IsPassEnabled(const RenderPass) const -> bool;
  auto SetPassEnabled(const RenderPass, const bool) -> void;
  /// @brief What the active backend's resource pools are holding. Empty where it has none.
  auto GetPoolUsage() const -> PoolUsage;
  /// @brief Curve the tone mapping pass fits the scene's radiance onto the display with.
  ///
  /// The exposure feeding that curve is not here: it belongs to the active camera, which the
  /// renderer reads every frame. See `Camera::exposureMode`.
  auto GetToneMapper() const -> ToneMapper;
  auto SetToneMapper(const ToneMapper) -> void;
  /// @brief Bytes of texture staging the active backend keeps in flight before it waits.
  ///
  /// Zero on a backend that does not stage uploads itself. OpenGL is one: `glTextureSubImage2D`
  /// hands pixels to the driver, which does its own staging and pipelining, so there is no buffer
  /// whose lifetime the engine has to manage and nothing a budget here could bound. Callers should
  /// read zero as "this backend has no such knob" and offer no control at all, rather than showing
  /// one that silently does nothing.
  auto GetUploadBudget() const -> size_t;
  /// @brief Resizes the active backend's upload batch. Ignored where the concept does not apply.
  auto SetUploadBudget(const size_t) -> void;
  /// @brief What the active backend can do, or none of it when there is no renderer yet.
  auto GetCapabilities() const -> RendererCapabilities;
  auto SetRenderer(const RenderingAPI = RenderingAPI::OpenGL) -> void;
  /// @brief Whether the finished frame is drawn onto the window at the end of every render.
  ///
  /// On by default, so that an application which does nothing but run gets a picture. The editor
  /// turns it off: it composites the same target into a dockable panel through Dear ImGui, and
  /// blitting it across the whole window first would be work thrown away every frame.
  ///
  /// What gets presented is the graph's final output, which is also what `GetTarget("")` returns --
  /// so a debug view selected in the editor would be presented too, if the editor presented.
  auto IsPresentEnabled() const -> bool;
  auto SetPresentEnabled(const bool) -> void;
  /// @brief Requests a render resolution, reallocating only once the request stops moving.
  ///
  /// The editor derives this from the viewport's content region, which is itself sized from the
  /// render target, so the two chase each other. Window maximise and dock-splitter settling then
  /// sweep the width across hundreds of intermediate values in as many frames. Under OpenGL that
  /// is merely wasteful; under Direct3D 12 each step is a full GPU stall plus fresh committed
  /// resources, so requests are debounced here rather than in any one backend.
  ///
  /// A request within `RESOLUTION_CHANGE_THRESHOLD` of the live resolution is dropped outright.
  /// Anything larger becomes a pending candidate that must hold still for
  /// `RESOLUTION_DEBOUNCE_FRAMES` before it is applied, which collapses a sweep of hundreds of
  /// steps into a single reallocation once it settles.
  ///
  /// Two resolutions alternating forever would never settle and so would never be applied. That is
  /// the intended outcome rather than a gap: the oscillation is driven by this system resizing the
  /// target the editor measures, so declining to resize breaks the feedback loop and lets the
  /// request converge on its own.
  auto SetResolution(const int = 1920, const int = 1080) -> void;
private:
  /// @brief Edge difference, in pixels, below which two resolutions count as the same.
  static constexpr int RESOLUTION_CHANGE_THRESHOLD = 8;
  /// @brief How many frames a requested resolution must hold still before targets are reallocated.
  static constexpr size_t RESOLUTION_DEBOUNCE_FRAMES = 5;
  RenderGraphBuilder graphBuilder;
  std::unique_ptr<RenderGraph> renderGraph;
  Renderer *activeRenderer{};
  std::array<std::unique_ptr<Renderer>, static_cast<uint8_t>(RenderingAPI::Vulkan) + 1> renderers;
  size_t fps{};
  size_t frameCounter{};
  size_t pendingSinceFrame{};
  AssetID derivedSkyboxAsset{};
  int pendingWidth{};
  int pendingHeight{};
  bool hasPending{};
  bool presentEnabled{true};
  /// @brief The size every viewport-sized target in the graph is built at, before the window has
  /// said anything about its own.
  ///
  /// Seeded from the desktop rather than fixed: the graph is compiled in `Start`, which runs before
  /// the first frame asks the window for a surface size, and a seed that does not match the display
  /// means every target is allocated once at the wrong size and again a few frames later. See
  /// `DesktopSize`.
  int screenWidth{DesktopWidth()};
  int screenHeight{DesktopHeight()};
  /// @brief Derives a directional light from the scene's skybox and writes it into the scene.
  ///
  /// Runs before the renderer loads the scene, and must: the OpenGL backend frees a texture's
  /// pixel data once it has been uploaded, so the environment map is only readable on the CPU
  /// until that point.
  ///
  /// Does nothing when the skybox has no region meaningfully brighter than the rest, leaving any
  /// light authored in the scene authoritative. A smooth gradient sky is exactly that case.
  auto UpdateSkyboxLight(Scene &) -> void;
  /// @brief Whether two edge lengths are close enough to count as unchanged.
  static auto NearlyEqual(const int, const int) -> bool;
  /// @brief Feeds a requested resolution into the debounce, committing it once it settles.
  auto UpdatePendingResolution(const int, const int) -> void;
  /// @brief Applies a resolution and reallocates the viewport-sized targets.
  auto CommitResolution(const int, const int) -> void;
};
auto RenderingSystem::ForEachTarget(auto &&func) const -> void {
  if (renderGraph)
    renderGraph->ForEachTarget(std::forward<decltype(func)>(func));
}
} // namespace kuki
