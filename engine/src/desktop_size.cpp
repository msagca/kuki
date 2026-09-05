#include <GLFW/glfw3.h>
#include <desktop_size.hpp>
#include <utility>
namespace kuki {
namespace {
constexpr int FALLBACK_WIDTH = 1920;
constexpr int FALLBACK_HEIGHT = 1080;
} // namespace
auto DesktopSize() -> std::pair<int, int> {
  // Asked for every time rather than cached: a monitor can be swapped or its mode changed while the
  // process runs, and the calls behind this are a lookup apiece.
  if (glfwInit() == GLFW_TRUE)
    if (auto *primary = glfwGetPrimaryMonitor(); primary)
      if (const auto *mode = glfwGetVideoMode(primary); mode && mode->width > 0 && mode->height > 0)
        return {mode->width, mode->height};
  return {FALLBACK_WIDTH, FALLBACK_HEIGHT};
}
auto DesktopWidth() -> int {
  return DesktopSize().first;
}
auto DesktopHeight() -> int {
  return DesktopSize().second;
}
} // namespace kuki
