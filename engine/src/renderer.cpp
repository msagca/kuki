#include <render_pass.hpp>
#include <renderer.hpp>
#include <span>
#include <string>
namespace kuki {
auto Renderer::ExecutePass(const RenderPass pass, std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  switch (pass) {
  case RenderPass::AntiAliasing:
    return ApplyAntiAliasing(inputs, outputs);
  case RenderPass::BrightPassFilter:
    return ApplyBrightPassFilter(inputs, outputs);
  case RenderPass::BloomEffect:
    return ApplyBloomEffect(inputs, outputs);
  case RenderPass::BlurEffect:
    return ApplyBlurEffect(inputs, outputs);
  case RenderPass::GammaCorrection:
    return ApplyGammaCorrection(inputs, outputs);
  case RenderPass::Outline:
    return ApplyOutline(inputs, outputs);
  case RenderPass::ShadowMap:
    return CreateShadowMap(inputs, outputs);
  case RenderPass::SpotShadowMap:
    return CreateSpotShadowMap(inputs, outputs);
  default:
    return RenderScene(inputs, outputs);
  }
}
} // namespace kuki
