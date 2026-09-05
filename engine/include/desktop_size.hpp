#pragma once
#include <kuki_engine_export.h>
#include <utility>
namespace kuki {
/// @brief The primary monitor's current video mode, in screen coordinates.
///
/// The size an application opens and renders at unless it asks for another one. A fixed 1920x1080
/// used to stand in for this, and was only ever right on one class of display: everywhere else a
/// window that opens maximised drew its first frames at a size nothing on the machine had, then
/// resized once the window reported its own.
///
/// Initialises GLFW if nothing has yet, because this is read while an `ApplicationDescription` is
/// built and that happens before there is a window. Initialising twice is free -- GLFW returns
/// immediately once it is up -- and the application's own `glfwInit` still runs where it always did.
///
/// Falls back to 1920x1080 when there is no monitor to ask, which is what a machine with no display
/// attached looks like. A fallback rather than a failure: a headless run still wants a resolution to
/// render at.
KUKI_ENGINE_API auto DesktopSize() -> std::pair<int, int>;
KUKI_ENGINE_API auto DesktopWidth() -> int;
KUKI_ENGINE_API auto DesktopHeight() -> int;
} // namespace kuki
