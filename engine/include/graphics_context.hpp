#pragma once
#include <kuki_engine_export.h>
#include <memory>
#include <rendering_api.hpp>
#include <utility>
struct GLFWwindow;
namespace kuki {
/// @brief Owns everything backend specific about the presentation surface.
///
/// Covers the window hints an API needs before the window exists, the device or context brought up on top of it, and the per-frame present. `Application` drives this lifecycle and never names a graphics API itself.
class KUKI_ENGINE_API GraphicsContext {
public:
  virtual ~GraphicsContext() = default;
  /// @brief Creates the context for the requested backend, falling back to OpenGL when it is unavailable.
  ///
  /// The single place that knows every implemented backend; mirrors `RenderingSystem::SetRenderer`.
  static auto Create(const RenderingAPI) -> std::unique_ptr<GraphicsContext>;
  /// @brief Sets the API specific window hints. Called before the window is created.
  virtual auto ApplyWindowHints() const -> void = 0;
  /// @brief Brings up the device or context on an existing window.
  ///
  /// @return False when the backend is unusable on this machine, which aborts startup.
  virtual auto Initialize(GLFWwindow *) -> bool = 0;
  /// @brief Opens the frame before any system renders into it.
  ///
  /// Immediate-mode backends have nothing to do here. Explicit ones reset the frame's command
  /// allocator, reopen the command list and wait until the GPU has finished with them, which is
  /// why this has to run before `UpdateSystems` rather than inside the renderer.
  virtual auto BeginFrame() -> void {}
  /// @brief Submits and presents the frame that was just rendered.
  virtual auto Present() -> void = 0;
  /// @brief Reacts to a framebuffer resize, in pixels.
  ///
  /// Not virtual, so that the size is recorded in exactly one place before the backend is told.
  /// `GetSurfaceSize` is what the present blit scales the finished image to, and a backend that
  /// forgot to record it here would blit to a stale rectangle rather than fail.
  auto Resize(const int width, const int height) -> void {
    surfaceWidth = width;
    surfaceHeight = height;
    OnResize(width, height);
  }
  /// @brief Size of the presentation surface in pixels, or zeroes before there is one.
  auto GetSurfaceSize() const -> std::pair<int, int> {
    return {surfaceWidth, surfaceHeight};
  }
  /// @brief Whether a present waits for the display's refresh. Backends that cannot, ignore it.
  virtual auto SetVSync(const bool) -> void {}
  /// @brief Tears the device or context down while the window is still alive.
  virtual auto Shutdown() -> void = 0;
protected:
  GraphicsContext() = default;
  /// @brief What `Resize` calls once it has recorded the new size.
  virtual auto OnResize(const int, const int) -> void = 0;
  GLFWwindow *window{};
  int surfaceWidth{};
  int surfaceHeight{};
};
} // namespace kuki
