#include <gl_context.hpp>
#include <graphics_context.hpp>
#include <memory>
#include <rendering_api.hpp>
#include <spdlog/spdlog.h>
#ifdef KUKI_HAS_DIRECTX
#include <dx_context.hpp>
#endif
namespace kuki {
auto GraphicsContext::Create(const RenderingAPI api) -> std::unique_ptr<GraphicsContext> {
  switch (api) {
  case RenderingAPI::DirectX:
#ifdef KUKI_HAS_DIRECTX
    return std::make_unique<DXContext>();
#else
    spdlog::warn("[GraphicsContext] DirectX is not available in this build, falling back to OpenGL");
    return std::make_unique<GLContext>();
#endif
  case RenderingAPI::Vulkan:
    spdlog::warn("[GraphicsContext] Vulkan is not implemented, falling back to OpenGL");
    [[fallthrough]];
  default:
    return std::make_unique<GLContext>();
  }
}
} // namespace kuki
