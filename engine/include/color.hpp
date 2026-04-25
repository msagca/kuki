#pragma once
#include <cstdint>
namespace kuki {
enum class ColorRange : uint8_t {
  LDR,
  HDR
};
enum class ColorSpace : uint8_t {
  Linear,
  sRGB
};
} // namespace kuki
