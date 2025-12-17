#pragma once
#include <kuki_engine_export.h>
#include <pool.hpp>
#include <texture_params.hpp>
namespace kuki {
class KUKI_ENGINE_API TexturePool final : public Pool<TextureParams, unsigned int> {
protected:
  unsigned int Allocate(const TextureParams&) override;
  void Reallocate(const TextureParams&, unsigned int&) override;
public:
  ~TexturePool() override;
  void Clear() override;
};
} // namespace kuki
