#pragma once
#include <kuki_engine_export.h>
#include <object_pool.hpp>
namespace kuki {
class KUKI_ENGINE_API FramebufferPool final : public ObjectPool<unsigned int> {
public:
  auto Clear() -> void;
protected:
  auto Allocate() -> unsigned int override;
};
} // namespace kuki
