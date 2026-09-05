#pragma once
#include <cstdint>
namespace kuki {
enum class RenderingAPI : uint8_t {
  DirectX,
  OpenGL,
  Vulkan
};
} // namespace kuki
