#pragma once
#include <kuki_engine_export.h>
#include <render_target.hpp>
#include <target_description.hpp>
#include <texture_content.hpp>
namespace kuki {
struct KUKI_ENGINE_API GLTexture final : public RenderTarget {
  unsigned int id{};
  TextureContent content{TextureContent::Albedo};
  bool flipY{false};
  explicit operator bool() const;
  auto GetTextureHandle() const -> uint64_t override {
    return id;
  }
  /// @brief `flipY` marks source data already stored bottom-up, which cancels out OpenGL's own flip.
  auto NeedsVerticalFlip() const -> bool override {
    return !flipY;
  }
};
} // namespace kuki
