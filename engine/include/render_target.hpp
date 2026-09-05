#pragma once
#include <cstdint>
#include <kuki_engine_export.h>
#include <target_description.hpp>
namespace kuki {
/// @brief Base for anything a render pass can draw into and a later pass, or the editor, can display.
class KUKI_ENGINE_API RenderTarget {
public:
  virtual ~RenderTarget() = default;
  /// @brief Backend handle for the colour texture, widened to the type ImGui takes.
  ///
  /// A texture name under OpenGL, a GPU descriptor handle under D3D12.
  ///
  /// @return Zero when the target has nothing displayable.
  virtual auto GetTextureHandle() const -> uint64_t {
    return 0;
  }
  /// @brief Whether display code has to flip V to show this target the right way up.
  ///
  /// Bottom-left texel origin is an OpenGL detail, so callers ask the target instead of assuming a convention.
  virtual auto NeedsVerticalFlip() const -> bool {
    return false;
  }
  TargetDescription desc;
protected:
  RenderTarget() = default;
};
} // namespace kuki
