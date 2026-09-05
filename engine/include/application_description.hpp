#pragma once
#include <engine_config.hpp>
#include <filesystem>
#include <rendering_api.hpp>
#include <string>
namespace kuki {
struct ApplicationDescription {
  std::string name;
  std::filesystem::path path;
  std::filesystem::path iconPath;
  RenderingAPI api{ConfiguredAPI()};
  /// @brief Size of the window, in screen coordinates, when it is neither maximised nor fullscreen.
  ///
  /// Ignored under `fullscreen`, which takes the monitor's current video mode instead, and largely
  /// ignored under `maximized`, which is why both of those are separate flags rather than sizes.
  int width{1920};
  int height{1080};
  /// @brief Opens maximised on the primary monitor. The editor's long-standing behaviour, so it is
  /// the default; a game that wants the size above should turn it off.
  bool maximized{true};
  /// @brief Opens borderless on the primary monitor at its current video mode.
  ///
  /// Takes precedence over `maximized`. Borderless rather than exclusive: exclusive fullscreen
  /// changes the display's mode, which costs a mode switch on alt-tab and is a poor default for a
  /// window that is mostly going to be developed against.
  bool fullscreen{};
  /// @brief Waits for the display's refresh before presenting.
  ///
  /// On by default, which is the right answer for anything shipping: a scene this small otherwise
  /// runs the GPU flat out to draw frames nobody sees. Turn it off to measure frame cost.
  bool vsync{true};
};
} // namespace kuki
