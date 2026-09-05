#pragma once
#include <graphics_context.hpp>
#include <kuki_engine_export.h>
namespace kuki {
/// @brief OpenGL 4.6 presentation surface: a GLFW-owned context, the global pipeline state, and buffer swapping.
class KUKI_ENGINE_API GLContext final : public GraphicsContext {
public:
  auto ApplyWindowHints() const -> void override;
  auto Initialize(GLFWwindow *) -> bool override;
  auto Present() -> void override;
  auto SetVSync(const bool) -> void override;
  auto Shutdown() -> void override;
private:
  auto OnResize(const int, const int) -> void override;
  static auto DebugMessageCallback(unsigned int, unsigned int, unsigned int, unsigned int, int, const char *, const void *) -> void;
};
} // namespace kuki
