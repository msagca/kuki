#pragma once
#ifdef KUKI_HAS_DIRECTX
#include <cstdint>
#include <dx_common.hpp>
#include <kuki_engine_export.h>
#include <render_target.hpp>
namespace kuki {
/// @brief Presents an already-uploaded texture through the `RenderTarget` interface.
///
/// Asset previews reach the editor as `RenderTarget *` because the OpenGL backend renders meshes
/// and materials into one. Previewing a texture needs no rendering at all, only the shader resource
/// view the texture already owns, so this carries the descriptor handle and owns no resource.
///
/// Lifetime therefore belongs to the `DXTexture` this was copied from. Anything that releases a
/// texture has to drop the views built from it in the same breath.
struct KUKI_ENGINE_API DXTextureView final : RenderTarget {
  D3D12_GPU_DESCRIPTOR_HANDLE srvGPU{};
  auto GetTextureHandle() const -> uint64_t override {
    return srvGPU.ptr;
  }
};
} // namespace kuki
#endif
