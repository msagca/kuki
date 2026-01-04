#pragma once
#include <cstdint>
namespace kuki {
enum class LightType : uint8_t {
  Directional,
  Point,
  Spot
};
}
