#pragma once
#include <cstdint>
namespace kuki {
enum class TextureType : uint8_t {
  Cubemap,
  Equirectangular,
  UV2D
};
} // namespace kuki
