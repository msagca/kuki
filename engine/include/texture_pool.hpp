#pragma once
#include <keyed_pool.hpp>
#include <kuki_engine_export.h>
#include <target_description.hpp>
namespace kuki {
class KUKI_ENGINE_API TexturePool final : public KeyedPool<TargetDescription, unsigned int> {
public:
  ~TexturePool() override;
  auto Clear() -> void;
  auto Reallocate(const TargetDescription &, unsigned int &) -> void override;
protected:
  auto Allocate(const TargetDescription &) -> unsigned int override;
};
} // namespace kuki
