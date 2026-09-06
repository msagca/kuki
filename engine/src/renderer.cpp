#include <render_pass.hpp>
#include <renderer.hpp>
#include <span>
#include <string>
namespace kuki {
auto Renderer::ExecutePass(const RenderPass pass, std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  if (!IsPassEnabled(pass))
    return BypassPass(pass, inputs, outputs);
  switch (pass) {
  case RenderPass::AntiAliasing:
    return ApplyAntiAliasing(inputs, outputs);
  case RenderPass::BrightPassFilter:
    return ApplyBrightPassFilter(inputs, outputs);
  case RenderPass::BloomEffect:
    return ApplyBloomEffect(inputs, outputs);
  case RenderPass::BlurEffect:
    return ApplyBlurEffect(inputs, outputs);
  case RenderPass::ToneMapping:
    return ApplyToneMapping(inputs, outputs);
  case RenderPass::Outline:
    return ApplyOutline(inputs, outputs);
  case RenderPass::Overlay:
    return ApplyOverlay(inputs, outputs);
  case RenderPass::DepthPrepass:
    return CreateDepthPrepass(inputs, outputs);
  case RenderPass::ShadowMap:
    return CreateShadowMap(inputs, outputs);
  case RenderPass::SpotShadowMap:
    return CreateSpotShadowMap(inputs, outputs);
  case RenderPass::ProbeTrace:
    return TraceProbes(inputs, outputs);
  default:
    return RenderScene(inputs, outputs);
  }
}
auto Renderer::BypassPass(const RenderPass pass, std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  switch (pass) {
  // Nothing at all. The probe field is not a target and not remade each frame, so standing this
  // down leaves it exactly where it was -- which is what "off" should mean for a running average:
  // the light stops following the scene, and what it had already gathered stays to be looked at.
  case RenderPass::ProbeTrace:
    return;
  // Also nothing, and for a related reason: these two are read by nobody except the bloom pass, so
  // whatever they hold while it is bypassed is never sampled. Clearing them would be work done to
  // produce a value no one reads. Bloom itself is where turning bloom off is felt.
  case RenderPass::BrightPassFilter:
  case RenderPass::BlurEffect:
    return;
  // Emptied. A depth target clears to the far plane, which is the honest neutral for all three: a
  // shadow map that reaches nothing shadows nothing, and a prepass that found no surfaces occludes
  // none. The scene pass is here too, and clearing it is the whole of what disabling it can mean.
  case RenderPass::DepthPrepass:
  case RenderPass::ShadowMap:
  case RenderPass::SpotShadowMap:
  case RenderPass::Scene:
    return BypassClear(outputs);
  // Everything else stands in the middle of a chain and is handed a picture to work on, so passing
  // that picture through unaltered is what not running amounts to. It is also the whole of what a
  // few of them do when they have nothing to say -- the outline pass already blits its input
  // through when nothing is selected, and this is that path reached from the other direction.
  default:
    return BypassCopy(inputs, outputs);
  }
}
} // namespace kuki
