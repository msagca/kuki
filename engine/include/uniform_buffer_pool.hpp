#pragma once
#include <buffer_params.hpp>
#include <kuki_engine_export.h>
#include <pool.hpp>
namespace kuki {
class KUKI_ENGINE_API UniformBufferPool final : public Pool<BufferParams, unsigned int> {
protected:
  unsigned int Allocate(const BufferParams&) override;
  void Reallocate(const BufferParams&, unsigned int&) override;
public:
  ~UniformBufferPool() override;
  void Clear() override;
};
} // namespace kuki
