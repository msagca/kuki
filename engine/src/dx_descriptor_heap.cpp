#include <dx_descriptor_heap.hpp>
#ifdef KUKI_HAS_DIRECTX
#include <dx_common.hpp>
#include <spdlog/spdlog.h>
namespace kuki {
auto DXDescriptorHeap::Initialize(ID3D12Device *device, const D3D12_DESCRIPTOR_HEAP_TYPE type, const uint32_t count, const bool visible) -> bool {
  if (!device || count == 0)
    return false;
  shaderVisible = visible && (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
  D3D12_DESCRIPTOR_HEAP_DESC desc{};
  desc.Type = type;
  desc.NumDescriptors = count;
  desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  if (DXFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)), "CreateDescriptorHeap"))
    return false;
  descriptorSize = device->GetDescriptorHandleIncrementSize(type);
  capacity = count;
  highWater = 0;
  freeList.clear();
  cpuStart = heap->GetCPUDescriptorHandleForHeapStart();
  gpuStart = shaderVisible ? heap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{};
  return true;
}
auto DXDescriptorHeap::Allocate() -> uint32_t {
  if (!freeList.empty()) {
    const auto index = freeList.back();
    freeList.pop_back();
    return index;
  }
  if (highWater >= capacity) {
    spdlog::error("[DX12] descriptor heap exhausted at {} descriptors.", capacity);
    return InvalidIndex;
  }
  return highWater++;
}
auto DXDescriptorHeap::AllocateRange(const uint32_t count) -> uint32_t {
  if (count == 0)
    return InvalidIndex;
  if (auto it = freeRanges.find(count); it != freeRanges.end() && !it->second.empty()) {
    const auto first = it->second.back();
    it->second.pop_back();
    return first;
  }
  if (highWater + count > capacity) {
    spdlog::error("[DX12] descriptor heap exhausted at {} descriptors, could not take a range of {}.", capacity, count);
    return InvalidIndex;
  }
  const auto first = highWater;
  highWater += count;
  return first;
}
auto DXDescriptorHeap::Free(const uint32_t index) -> void {
  if (index == InvalidIndex || index >= highWater)
    return;
  freeList.push_back(index);
}
auto DXDescriptorHeap::FreeRange(const uint32_t first, const uint32_t count) -> void {
  if (first == InvalidIndex || count == 0 || first + count > highWater)
    return;
  freeRanges[count].push_back(first);
}
auto DXDescriptorHeap::GetHighWater() const -> uint32_t {
  return highWater;
}
auto DXDescriptorHeap::GetCapacity() const -> uint32_t {
  return capacity;
}
auto DXDescriptorHeap::GetCPUHandle(const uint32_t index) const -> D3D12_CPU_DESCRIPTOR_HANDLE {
  D3D12_CPU_DESCRIPTOR_HANDLE handle = cpuStart;
  handle.ptr += static_cast<SIZE_T>(index) * descriptorSize;
  return handle;
}
auto DXDescriptorHeap::GetGPUHandle(const uint32_t index) const -> D3D12_GPU_DESCRIPTOR_HANDLE {
  D3D12_GPU_DESCRIPTOR_HANDLE handle = gpuStart;
  handle.ptr += static_cast<UINT64>(index) * descriptorSize;
  return handle;
}
auto DXDescriptorHeap::Get() const -> ID3D12DescriptorHeap * {
  return heap.Get();
}
auto DXDescriptorHeap::GetDescriptorSize() const -> uint32_t {
  return descriptorSize;
}
auto DXDescriptorHeap::IsShaderVisible() const -> bool {
  return shaderVisible;
}
auto DXDescriptorHeap::Reset() -> void {
  freeList.clear();
  highWater = 0;
}
auto DXDescriptorHeap::Shutdown() -> void {
  Reset();
  heap.Reset();
  capacity = 0;
  descriptorSize = 0;
  cpuStart = {};
  gpuStart = {};
}
} // namespace kuki
#endif
