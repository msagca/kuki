#pragma once
#include <keyed_pool.hpp>
#include <kuki_engine_export.h>
namespace kuki {
class KUKI_ENGINE_API BufferPool final : public KeyedPool<int, unsigned int> {
public:
  auto Clear() -> void;
  auto Reallocate(const int &, unsigned int &) -> void override;
protected:
  auto Allocate(const int & = 0) -> unsigned int override;
};
} // namespace kuki
