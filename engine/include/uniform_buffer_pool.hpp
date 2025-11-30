#pragma once
#include <buffer_params.hpp>
#include <gl_constants.hpp>
#include <kuki_engine_export.h>
#include <pool.hpp>
namespace kuki {
class KUKI_ENGINE_API UniformBufferPool final : public Pool<BufferParams, GLConst::UINT> {
protected:
  GLConst::UINT Allocate(const BufferParams&) override;
  void Reallocate(const BufferParams&, GLConst::UINT&) override;
public:
  ~UniformBufferPool() override;
  void Clear() override;
};
} // namespace kuki
