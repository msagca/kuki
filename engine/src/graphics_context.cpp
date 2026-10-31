#include <graphics_context.hpp>
#include <memory>
#include <rendering_api.hpp>
#include <spdlog/spdlog.h>
#ifdef KUKI_HAS_DIRECTX
#include <dx_context.hpp>
#endif
namespace kuki {
auto GraphicsContext::Create(const RenderingAPI api) -> std::unique_ptr<GraphicsContext> {
  // No fallback: there is one backend, and a context that cannot draw what the engine draws is no
  // answer -- OpenGL was the fallback once, and by the time it went it drew no meshes at all. A null
  // context makes `Application` stop with this reason, and a second backend adds its case here.
  switch (api) {
  case RenderingAPI::DirectX:
#ifdef KUKI_HAS_DIRECTX
    return std::make_unique<DXContext>();
#else
    spdlog::error("[GraphicsContext] Direct3D 12 is not available in this build, and there is no other backend");
    return nullptr;
#endif
  case RenderingAPI::Vulkan:
    spdlog::error("[GraphicsContext] Vulkan has no backend yet");
    return nullptr;
  }
  return nullptr;
}
} // namespace kuki
