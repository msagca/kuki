#pragma once
#include <kuki_engine_export.h>
#include <render_target.hpp>
namespace kuki {
struct KUKI_ENGINE_API GLFramebuffer final : public RenderTarget {
  GLFramebuffer();
  unsigned int id{};
  explicit operator bool() const;
};
} // namespace kuki
