#pragma once
#include <kuki_engine_export.h>
#include <object_pool.hpp>
namespace kuki {
class KUKI_ENGINE_API GLFramebufferPool final : public ObjectPool<unsigned int> {
protected:
  auto Allocate() -> unsigned int override;
  auto Deallocate(unsigned int &) -> void override;
};
} // namespace kuki
