#pragma once
#ifdef KUKI_HAS_DIRECTX
#include <dx_common.hpp>
#include <dx_descriptor_heap.hpp>
#include <vector>
namespace kuki {
/// @brief A texture a compute shader writes and a later pass samples.
///
/// Distinct from `DXRenderTarget` because nothing here is ever bound as a render target, and
/// because the mip chain matters: the image-based lighting passes write one mip at a time and read
/// the level below it, so every level needs its own view rather than one view of the whole chain.
///
/// `state` is tracked for the resource as a whole. The mip chain is the one place that breaks: the
/// downsample walks the levels with per-subresource barriers, leaving the resource in a mixed state
/// mid-build, and restores a uniform state before anything samples it.
///
/// Every index here refers to a processor-side staging heap, not the shader-visible one. A
/// descriptor can only be copied out of a heap the shaders cannot see, so views are authored there
/// and copied into whichever shader-visible block needs them.
struct DXComputeTexture {
  ComPtr<ID3D12Resource> resource;
  D3D12_RESOURCE_STATES state{D3D12_RESOURCE_STATE_COMMON};
  /// @brief View of the whole chain, as a cube when the texture has six faces.
  uint32_t srvIndex{DXDescriptorHeap::InvalidIndex};
  /// @brief Array views of single mips, which the downsample reads its source level through.
  std::vector<uint32_t> mipSrvIndices;
  /// @brief Unordered access views, one per mip, which the compute passes write through.
  std::vector<uint32_t> mipUavIndices;
  uint32_t size{};
  uint32_t mipLevels{1};
  uint32_t arraySize{1};
  /// @brief Slices, for a texture that is a volume rather than an image or an array of them.
  ///
  /// One everywhere except the cloud noise, which is genuinely three dimensional: a cloud is
  /// sampled at a position in the air, and a stack of slices would have to be filtered between by
  /// hand at every one of the several dozen fetches a marched pixel takes. Left at one, the views
  /// built over a texture are the two-dimensional ones every other user of this struct wants.
  uint32_t depth{1};
  explicit operator bool() const {
    return resource && srvIndex != DXDescriptorHeap::InvalidIndex;
  }
};
} // namespace kuki
#endif
