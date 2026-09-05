#pragma once
#include <kuki_engine_export.h>
#include <render_target.hpp>
namespace kuki {
struct KUKI_ENGINE_API GLRenderTarget final : public RenderTarget {
  unsigned int framebuffer{};
  unsigned int renderbuffer{};
  unsigned int texture{};
  unsigned int idTexture{};
  auto GetTextureHandle() const -> uint64_t override {
    return texture;
  }
  auto NeedsVerticalFlip() const -> bool override {
    return true;
  }
};
} // namespace kuki
