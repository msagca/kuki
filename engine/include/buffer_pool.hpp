#pragma once
#include <buffer_description.hpp>
#include <kuki_engine_export.h>
#include <pool.hpp>
namespace kuki {
class KUKI_ENGINE_API BufferPool final : public Pool<BufferDescription, unsigned int> {
public:
  ~BufferPool() override;
  auto Clear() -> void;
protected:
  auto Allocate(const BufferDescription &) -> unsigned int override;
  auto Reallocate(const BufferDescription &, unsigned int &) -> void override;
};
} // namespace kuki
