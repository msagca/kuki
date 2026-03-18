#pragma once
#include <kuki_engine_export.h>
#include <render_target.hpp>
#include <target_description.hpp>
#include <texture_content.hpp>
namespace kuki {
struct KUKI_ENGINE_API GLTexture final : public RenderTarget {
  unsigned int id{};
  TextureContent content{TextureContent::Unknown};
  TargetDescription desc{};
  explicit operator bool() const;
};
} // namespace kuki
