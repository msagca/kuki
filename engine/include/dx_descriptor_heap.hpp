#pragma once
#ifdef KUKI_HAS_DIRECTX
#include <dx_common.hpp>
#include <kuki_engine_export.h>
#include <unordered_map>
#include <vector>
namespace kuki {
/// @brief Fixed-capacity descriptor heap with index-based allocation and a free list.
///
/// Descriptors are handed out as plain indices so resources can store a stable `uint32_t` rather
/// than a handle that would dangle if the heap were ever recreated. Shader-visible heaps are the
/// ones a root signature binds; CPU-only heaps back RTVs and DSVs.
class KUKI_ENGINE_API DXDescriptorHeap {
public:
  static constexpr uint32_t InvalidIndex = static_cast<uint32_t>(-1);
  /// @brief Creates the underlying heap.
  /// @return False when creation failed, which is fatal for the backend.
  auto Initialize(ID3D12Device *, const D3D12_DESCRIPTOR_HEAP_TYPE, const uint32_t, const bool = false) -> bool;
  /// @brief Takes the next free descriptor slot.
  /// @return Slot index, or `InvalidIndex` when the heap is full.
  auto Allocate() -> uint32_t;
  /// @brief Takes several adjacent slots at once, for a descriptor table the shader indexes into.
  ///
  /// A descriptor range in a root signature is defined by its first slot and a count, so the
  /// descriptors behind it have to be neighbours. The free list of single slots is in whatever
  /// order they were released and cannot answer that, so ranges are kept and recycled whole, in
  /// their own list per length.
  ///
  /// Per length, and matched exactly, because there are three lengths in this engine and they are
  /// compile-time constants. Splitting a longer free range to satisfy a shorter request, or joining
  /// neighbours back together, would be a general allocator solving a problem nothing here has.
  ///
  /// @return Index of the first slot, or `InvalidIndex` when nothing of that length is free and the
  /// remaining capacity is too small.
  auto AllocateRange(const uint32_t) -> uint32_t;
  /// @brief Returns a slot to the free list. Safe to call with `InvalidIndex`.
  auto Free(const uint32_t) -> void;
  /// @brief Returns a range whole, so a later request of the same length can have it.
  ///
  /// Whole rather than scattered into the single-slot list, which is what this used to do: broken
  /// up, the slots could only ever come back one at a time, and the heap would report itself
  /// exhausted to a table that needed seven neighbours while thousands of slots stood free.
  ///
  /// Safe to call with `InvalidIndex`. The caller owns the timing: a descriptor table is read when
  /// the command list executes rather than when it is recorded, so a range must not come back here
  /// until every list that named it has run.
  auto FreeRange(const uint32_t, const uint32_t) -> void;
  /// @brief Slots ever taken from fresh capacity, and how many there are in total.
  ///
  /// The first is what exhaustion is measured against: recycled slots do not raise it, so a figure
  /// that climbs while the scene stands still is a leak rather than a busy frame.
  auto GetHighWater() const -> uint32_t;
  auto GetCapacity() const -> uint32_t;
  auto GetCPUHandle(const uint32_t) const -> D3D12_CPU_DESCRIPTOR_HANDLE;
  /// @brief GPU-side handle for a slot. Only meaningful on a shader-visible heap.
  auto GetGPUHandle(const uint32_t) const -> D3D12_GPU_DESCRIPTOR_HANDLE;
  auto Get() const -> ID3D12DescriptorHeap *;
  auto GetDescriptorSize() const -> uint32_t;
  auto IsShaderVisible() const -> bool;
  /// @brief Drops every allocation without destroying the heap.
  auto Reset() -> void;
  /// @brief Releases the heap itself.
  auto Shutdown() -> void;
private:
  ComPtr<ID3D12DescriptorHeap> heap;
  std::vector<uint32_t> freeList;
  /// @brief Released ranges, kept whole and grouped by their length.
  std::unordered_map<uint32_t, std::vector<uint32_t>> freeRanges;
  D3D12_CPU_DESCRIPTOR_HANDLE cpuStart{};
  D3D12_GPU_DESCRIPTOR_HANDLE gpuStart{};
  uint32_t descriptorSize{};
  uint32_t capacity{};
  uint32_t highWater{};
  bool shaderVisible{};
};
} // namespace kuki
#endif
