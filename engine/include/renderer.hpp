#pragma once
#include <array>
#include <buffer_object.hpp>
#include <debug_view.hpp>
#include <exposure.hpp>
#include <indirect_lighting.hpp>
#include <kuki_engine_export.h>
#include <material_type.hpp>
#include <pool_policy.hpp>
#include <post_process.hpp>
#include <render_graph.hpp>
#include <render_pass.hpp>
#include <render_target.hpp>
#include <renderer_capabilities.hpp>
#include <scene.hpp>
#include <scene_manager.hpp>
#include <shader.hpp>
#include <shader_asset.hpp>
#include <string>
#include <target_description.hpp>
#include <tone_mapper.hpp>
namespace kuki {
class KUKI_ENGINE_API Renderer {
public:
  virtual ~Renderer() = default;
  virtual auto ApplyAntiAliasing(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto ApplyBloomEffect(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto ApplyBlurEffect(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto ApplyBrightPassFilter(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto ApplyToneMapping(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto Clear() -> void = 0;
  virtual auto CreateDepthPrepass(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto CreateShadowMap(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto CreateSpotShadowMap(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto CreateTarget(const TargetDescription &, const std::string & = "") -> EntityID = 0;
  virtual auto GetPreviewSize() const -> int = 0;
  virtual auto GetTarget(const std::string &) -> RenderTarget * = 0;
  virtual auto LoadAsset(const AssetID) -> void = 0;
  virtual auto LoadAssets(const AssetType) -> void = 0;
  virtual auto ApplyOutline(std::span<std::string>, std::span<std::string>) -> void = 0;
  virtual auto LoadScene(Scene &) -> void = 0;
  virtual auto PickEntity(const int, const int) -> EntityID = 0;
  virtual auto PreviewAsset(const AssetID) -> RenderTarget * = 0;
  virtual auto SetPreviewSize(const int) -> void = 0;
  virtual auto RenderScene(std::span<std::string>, std::span<std::string>) -> void = 0;
  /// @brief Fills the irradiance probe field for this frame, where the backend keeps one.
  ///
  /// A pass rather than a step inside the scene pass, because the field is an input to shading in
  /// the same way a shadow map is: it has to be finished before anything is shaded with it, and
  /// that is worth stating once as a dependency instead of arranging by hand at every call site.
  /// Backends without a probe volume do nothing here.
  virtual auto TraceProbes(std::span<std::string>, std::span<std::string>) -> void = 0;
  /// @brief Draws a finished target onto the window's own surface, scaled to fill it.
  ///
  /// The render graph ends in an offscreen target, which is the right shape for an editor: the
  /// image is a texture it puts inside a panel. An application without a panel needs the same
  /// image on the swapchain, and until this existed there was nothing to put it there -- both
  /// backends presented whatever the surface happened to hold, which is to say nothing.
  ///
  /// Defaults to doing nothing so that a backend without a route to its own surface degrades to a
  /// blank window rather than failing to build. Both implemented backends override it.
  virtual auto PresentTarget(const std::string &) -> void {}
  virtual auto Reset() -> void = 0;
  virtual auto SetResolution(const int = 1920, const int = 1080) -> void = 0;
  virtual auto UpdateTarget(const std::string &, const TargetDescription &) -> void = 0;
  auto ExecutePass(const RenderPass, std::span<std::string>, std::span<std::string>) -> void;
  /// @brief Whether a pass runs this frame, or is stood down and its output made harmless.
  ///
  /// Off is not the same as absent. The graph still executes a disabled pass, and what it executes
  /// is a bypass that leaves the pass's output saying what the rest of the frame needs it to say --
  /// a copy of the input for a pass in the middle of a chain, an empty target for one that starts
  /// it. Dropping the pass out of the graph instead would leave its consumers reading last frame's
  /// picture, which looks like a working effect rather than a disabled one.
  ///
  /// Deliberately not saved with the display settings, for the reason the debug views are not: a
  /// pipeline that comes back half switched off on the next launch is indistinguishable from a bug.
  auto IsPassEnabled(const RenderPass pass) const -> bool { return passEnabled[static_cast<size_t>(pass)]; }
  auto SetPassEnabled(const RenderPass pass, const bool enabled) -> void { passEnabled[static_cast<size_t>(pass)] = enabled; }
  /// @brief What the backend's resource pools are holding. Empty where the backend has none.
  virtual auto GetPoolUsage() const -> PoolUsage { return {}; }
  /// @brief What this backend can do, so the editor offers only controls that reach something.
  ///
  /// Defaults to none of it. See `RendererCapabilities` for why that is the safe direction.
  virtual auto GetCapabilities() const -> RendererCapabilities { return {}; }
  /// @brief Curve the tone mapping pass fits the scene onto the display with.
  auto GetToneMapper() const -> ToneMapper { return toneMapper; }
  auto SetToneMapper(const ToneMapper mapper) -> void { toneMapper = mapper; }
  /// @brief Which step of the shading the scene pass writes out instead of the finished pixel.
  ///
  /// Not a pass and not a target: every one of these is an intermediate inside the pixel shader, and
  /// the only way to see one is to have the shader write it. Backends whose shading does not compute
  /// a given step ignore the request for it.
  auto GetLightingDebugView() const -> LightingDebugView { return lightingDebugView; }
  auto SetLightingDebugView(const LightingDebugView view) -> void { lightingDebugView = view; }
  /// @brief What the probe spheres are coloured by, or `Off` for no spheres at all.
  ///
  /// A view rather than a pass, for the same reason: the spheres are drawn into the scene image
  /// alongside the geometry, so there is no target to select and nothing in the graph to switch.
  /// Backends without a probe volume ignore it.
  auto GetProbeDebugView() const -> ProbeDebugView { return probeDebugView; }
  auto SetProbeDebugView(const ProbeDebugView view) -> void { probeDebugView = view; }
  /// @brief What shapes the indirect lighting, held here for the reason the tone curve is.
  ///
  /// Neither backend owns these: they are read into a constant buffer on one side and a uniform on
  /// the other, and a backend swapped at runtime has to carry them across.
  ///
  /// The DirectX backend reads all of them. The OpenGL one reads the two terms it has a route for
  /// -- `skyIntensity` scales its irradiance cubemap and `ambientFallback` is the flat ambient it
  /// falls back on -- and ignores everything from `bounceIntensity` down, all of which describes a
  /// probe field it does not have. `GetCapabilities().probeVolume` is what says which of those two
  /// a build is looking at, and the editor hides the rest rather than drawing sliders that reach
  /// nothing.
  auto GetIndirectLighting() const -> const IndirectLighting & { return indirect; }
  auto SetIndirectLighting(const IndirectLighting &settings) -> void { indirect = settings; }
  template <typename T>
  auto As(this auto &self) -> ConstCorrectPointer<decltype(self), T>;
  template <typename T>
  auto Is() const -> bool;
protected:
  template <typename T>
  Renderer(std::in_place_type_t<T>, Application &);
  Application &app;
  /// @brief Held here rather than in each backend because both post chains are ports of one another.
  ///
  /// Two copies of a display setting is two things to keep in step, and nothing about either value
  /// is specific to an API: they are read straight into a uniform on one side and a root constant on
  /// the other. Switching backends at runtime also has to carry them across, which it gets for free
  /// only while they live somewhere both backends already share.
  ///
  /// `exposure` is a cache rather than a setting. It belongs to the active camera and is refreshed
  /// from it every `LoadScene`, so writing to it from anywhere else lasts exactly one frame. There
  /// is deliberately no setter: the camera is the place to change it.
  float exposure{DEFAULT_EXPOSURE};
  ToneMapper toneMapper{DEFAULT_TONE_MAPPER};
  /// @brief Held here for the same reason as the two above, and deliberately not saved with them.
  ///
  /// A tone mapping curve is a preference and belongs in the config; a debug view is a thing you are
  /// in the middle of looking at. Restoring one on the next launch would leave the editor opening on
  /// a picture of a weight buffer with no obvious way back, which is a worse failure than having to
  /// pick the view again.
  LightingDebugView lightingDebugView{};
  ProbeDebugView probeDebugView{};
  IndirectLighting indirect{};
  std::array<bool, RENDER_PASS_COUNT> passEnabled{[] {
    std::array<bool, RENDER_PASS_COUNT> all{};
    all.fill(true);
    return all;
  }()};
  /// @brief Stands a pass down, leaving its output where the passes after it can still use it.
  ///
  /// Which of the three treatments below each pass gets is argued at the switch. Backends override
  /// this only to add what their own bookkeeping needs; the choice itself is shared, because it is
  /// a statement about what the pass means rather than about how either API draws.
  virtual auto BypassPass(const RenderPass, std::span<std::string>, std::span<std::string>) -> void;
  /// @brief Copies the most finished colour input to the output, resolving it if it is multisampled.
  virtual auto BypassCopy(std::span<std::string>, std::span<std::string>) -> void = 0;
  /// @brief Empties the outputs: colour to the backend's clear colour, depth to the far plane.
  virtual auto BypassClear(std::span<std::string>) -> void = 0;
private:
  std::type_index typeIndex;
};
template <typename T>
Renderer::Renderer(std::in_place_type_t<T>, Application &app)
  : typeIndex(typeid(T)), app(app) {}
template <typename T>
auto Renderer::As(this auto &self) -> ConstCorrectPointer<decltype(self), T> {
  if (self.template Is<T>())
    return static_cast<ConstCorrectPointer<decltype(self), T>>(&self);
  return nullptr;
}
template <typename T>
auto Renderer::Is() const -> bool {
  return typeIndex == typeid(T);
}
} // namespace kuki
