#pragma once
#include <cstdint>
namespace kuki {
enum class PrimitiveType : uint8_t {
  Cube,
  CubeInverted,
  Cylinder,
  Frame,
  Plane,
  Sphere
};
}
