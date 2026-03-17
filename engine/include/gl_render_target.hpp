#pragma once
#include <kuki_engine_export.h>
#include <render_target.hpp>
namespace kuki {
struct KUKI_ENGINE_API GLRenderTarget final : public RenderTarget {
  GLRenderTarget();
  unsigned int framebuffer{};
  unsigned int renderbuffer{};
  unsigned int texture{};
};
} // namespace kuki
