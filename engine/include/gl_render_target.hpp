#pragma once
#include <render_target.hpp>
namespace kuki {
struct GLRenderTarget final : public RenderTarget {
  unsigned int framebuffer{};
  unsigned int renderbuffer{};
  unsigned int texture{};
  unsigned int idTexture{};
};
} // namespace kuki
