#pragma once
#include <gl_constants.hpp>
#include <kuki_engine_export.h>
#include <texture_pool.hpp>
namespace kuki {
class KUKI_ENGINE_API FramebufferPool final : public Pool<TextureParams, GLConst::UINT> {
protected:
  GLConst::UINT Allocate(const TextureParams&) override;
public:
  ~FramebufferPool() override;
  void Clear() override;
};
} // namespace kuki
