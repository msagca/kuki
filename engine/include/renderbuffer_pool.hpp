#pragma once
#include <kuki_engine_export.h>
#include <pool.hpp>
#include <target_description.hpp>
namespace kuki {
class KUKI_ENGINE_API RenderbufferPool final : public Pool<TargetDescription, unsigned int> {
public:
  ~RenderbufferPool() override;
  auto Clear() -> void;
protected:
  auto Allocate(const TargetDescription &) -> unsigned int override;
  auto Reallocate(const TargetDescription &, unsigned int &) -> void override;
};
} // namespace kuki
