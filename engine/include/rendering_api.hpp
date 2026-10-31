#pragma once
#include <cstdint>
namespace kuki {
/// @brief The graphics APIs a backend can be written for.
///
/// Direct3D 12 is the only one with a backend. OpenGL had one and lost it: it had fallen so far
/// behind -- no pools, no probe field, no cel shading, no wind -- that falling back to it drew an
/// empty or wrong picture rather than a lesser one. Vulkan is named so that the places that choose a
/// backend (`GraphicsContext::Create`, `RenderingSystem::SetRenderer`, `IsAvailable`, the editor's
/// Settings panel) already have the case a second backend would fill. Saved by name in
/// `kuki.config.json`, never by number.
enum class RenderingAPI : uint8_t {
  DirectX,
  Vulkan
};
} // namespace kuki
