#pragma once
#include <kuki_engine_export.h>
#include <render_target.hpp>
#include <target_description.hpp>
namespace kuki {
struct KUKI_ENGINE_API GLTexture final : public RenderTarget {
  GLTexture();
  unsigned int id{};
  TargetDescription desc{};
  explicit operator bool() const;
};
} // namespace kuki
