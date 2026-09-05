#pragma once
#ifdef KUKI_HAS_DIRECTX
#include <dx_common.hpp>
#include <dx_descriptor_heap.hpp>
#include <texture_content.hpp>
namespace kuki {
/// @brief A sampled Direct3D 12 texture: the default-heap resource and its shader resource view.
///
/// `srvGPU` is cached because a draw binds the descriptor table by GPU handle, and recomputing it
/// from the heap on every draw would mean a lookup per material.
/// `format` is kept so the view can be recreated elsewhere in the heap. Material texture tables
/// need their descriptors adjacent, and a descriptor cannot be moved once written, so the view is
/// built a second time directly in the table's slot rather than copied there.
struct DXTexture {
  ComPtr<ID3D12Resource> resource;
  uint32_t srvIndex{DXDescriptorHeap::InvalidIndex};
  D3D12_GPU_DESCRIPTOR_HANDLE srvGPU{};
  DXGI_FORMAT format{DXGI_FORMAT_UNKNOWN};
  TextureContent content{TextureContent::Albedo};
  /// @brief Levels the resource actually holds, so a view rebuilt elsewhere covers the same chain.
  uint32_t mipLevels{1};
  /// @brief Swizzle the view applies, which is how a one-channel format answers a green or blue read.
  ///
  /// A shader asks occlusion, roughness and metalness for a different channel each, and BC4 keeps
  /// only red. Rather than teach the shader which textures are compressed, the view spreads red
  /// across the colour channels and the read lands on it whichever one it names.
  uint32_t shaderMapping{D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING};
  explicit operator bool() const {
    return resource && srvIndex != DXDescriptorHeap::InvalidIndex;
  }
};
} // namespace kuki
#endif
