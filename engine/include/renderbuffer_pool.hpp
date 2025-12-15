#pragma once
#include <kuki_engine_export.h>
#include <pool.hpp>
#include <texture_params.hpp>
namespace kuki {
class KUKI_ENGINE_API RenderbufferPool final : public Pool<TextureParams, unsigned int> {
protected:
  unsigned int Allocate(const TextureParams&) override;
  void Reallocate(const TextureParams&, unsigned int&) override;
public:
  ~RenderbufferPool() override;
  void Clear() override;
};
} // namespace kuki
