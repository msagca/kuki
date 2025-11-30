#pragma once
#include <gl_constants.hpp>
#include <kuki_engine_export.h>
#include <pool.hpp>
#include <texture_params.hpp>
namespace kuki {
class KUKI_ENGINE_API RenderbufferPool final : public Pool<TextureParams, GLConst::UINT> {
protected:
  GLConst::UINT Allocate(const TextureParams&) override;
  void Reallocate(const TextureParams&, GLConst::UINT&) override;
public:
  ~RenderbufferPool() override;
  void Clear() override;
};
} // namespace kuki
