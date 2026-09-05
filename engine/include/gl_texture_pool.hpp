#pragma once
#include <keyed_pool.hpp>
#include <kuki_engine_export.h>
#include <target_description.hpp>
namespace kuki {
class KUKI_ENGINE_API GLTexturePool final : public KeyedPool<TargetDescription, unsigned int> {
public:
  auto Reallocate(const TargetDescription &, unsigned int &) -> void override;
protected:
  auto Allocate(const TargetDescription &) -> unsigned int override;
  auto Deallocate(unsigned int &) -> void override;
};
} // namespace kuki
