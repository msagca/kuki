#pragma once
#include <gl_constants.hpp>
#include <kuki_engine_export.h>
#include <pool.hpp>
#include <texture_params.hpp>
namespace kuki {
class KUKI_ENGINE_API TexturePool final : public Pool<TextureParams, GLConst::UINT> {
private:
  GLConst::ENUM GetBaseFormat(GLConst::ENUM);
protected:
  GLConst::UINT Allocate(const TextureParams&) override;
  void Reallocate(const TextureParams&, GLConst::UINT&) override;
public:
  ~TexturePool() override;
  void Clear() override;
};
} // namespace kuki
