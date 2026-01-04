#pragma once
#include <cstdint>
namespace kuki {
enum class ShaderType : uint8_t {
  Compute,
  Fragment,
  Geometry,
  Vertex,
  Unknown
};
}
