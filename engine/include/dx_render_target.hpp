#pragma once
#ifdef KUKI_HAS_DIRECTX
#include <dx_common.hpp>
#include <dx_descriptor_heap.hpp>
#include <kuki_engine_export.h>
#include <render_target.hpp>
#include <vector>
namespace kuki {
/// @brief An offscreen Direct3D 12 target: the resource plus the views the passes bind it through.
///
/// `state` tracks the resource's current barrier state so a pass can transition it only when the
/// state actually changes. Unlike OpenGL, nothing infers this for us.
struct KUKI_ENGINE_API DXRenderTarget final : public RenderTarget {
  ComPtr<ID3D12Resource> resource;
  ComPtr<ID3D12Resource> idResource;
  ComPtr<ID3D12Resource> depthResource;
  uint32_t depthDsvIndex{DXDescriptorHeap::InvalidIndex};
  uint32_t rtvIndex{DXDescriptorHeap::InvalidIndex};
  uint32_t dsvIndex{DXDescriptorHeap::InvalidIndex};
  uint32_t srvIndex{DXDescriptorHeap::InvalidIndex};
  uint32_t idRtvIndex{DXDescriptorHeap::InvalidIndex};
  uint32_t idSrvIndex{DXDescriptorHeap::InvalidIndex};
  /// @brief One depth view per array slice, for targets a pass renders into a layer at a time.
  ///
  /// `dsvIndex` addresses every slice at once, which is what clearing wants. Drawing wants the
  /// opposite: a view of exactly one slice, since a draw cannot choose its render target slice
  /// without a geometry shader. Spot shadow maps are the case that needs both.
  std::vector<uint32_t> layerDsvIndices;
  D3D12_RESOURCE_STATES state{D3D12_RESOURCE_STATE_COMMON};
  D3D12_GPU_DESCRIPTOR_HANDLE srvGPU{};
  /// @brief GPU handle of the entity-id buffer's view, or zero when the target has no picking buffer.
  ///
  /// Sampled as `Texture2DMS` when the target is multisampled: entity ids must never be filtered,
  /// since averaging two ids yields one belonging to neither object. Readers take a single sample.
  D3D12_GPU_DESCRIPTOR_HANDLE idSrvGPU{};
  auto GetTextureHandle() const -> uint64_t override {
    return srvGPU.ptr;
  }
  /// @brief Direct3D samples from a top-left origin, so displaying a target needs no flip.
  auto NeedsVerticalFlip() const -> bool override {
    return false;
  }
};
} // namespace kuki
#endif
