#pragma once
#ifdef KUKI_HAS_DIRECTX
#include <array>
#include <dx_common.hpp>
#include <dx_descriptor_heap.hpp>
#include <graphics_context.hpp>
#include <kuki_engine_export.h>
#include <utility>
#include <vector>
namespace kuki {
/// @brief Direct3D 12 presentation surface: device, swap chain, command submission and frame pacing.
///
/// Owns the pieces every pass needs but none of the passes themselves. `DXRenderer` borrows the
/// device, the open command list and the descriptor heaps from here.
///
/// Frame pacing is the classic N-deep pattern: one command allocator per frame in flight, a single
/// fence, and a per-frame fence value. `BeginFrame` will not reset an allocator until the GPU has
/// signalled past the value recorded the last time that allocator was used.
class KUKI_ENGINE_API DXContext final : public GraphicsContext {
public:
  ~DXContext() override;
  auto ApplyWindowHints() const -> void override;
  auto Initialize(GLFWwindow *) -> bool override;
  auto BeginFrame() -> void override;
  auto Present() -> void override;
  auto SetVSync(const bool) -> void override;
  auto Shutdown() -> void override;
  auto OnResize(const int, const int) -> void override;
  auto GetDevice() const -> ID3D12Device *;
  /// @brief The same device through the interface that builds acceleration structures.
  ///
  /// Null when the driver predates raytracing, which is why it is a separate accessor rather than
  /// a widening of `GetDevice`: a caller that does not need raytracing should not have to care.
  auto GetDevice5() const -> ID3D12Device5 *;
  /// @brief What this adapter supports beyond the feature level, queried once at initialisation.
  auto GetCapabilities() const -> const DXCapabilities &;
  /// @brief The command list recorded between `BeginFrame` and `Present`. Null outside a frame.
  auto GetCommandList() const -> ID3D12GraphicsCommandList *;
  /// @brief The same command list through the interface that records raytracing work.
  ///
  /// Null outside a frame, and null always when the driver has no raytracing interface at all.
  auto GetCommandList4() const -> ID3D12GraphicsCommandList4 *;
  auto GetCommandQueue() const -> ID3D12CommandQueue *;
  auto GetFrameIndex() const -> uint32_t;
  /// @brief Shader-visible CBV/SRV/UAV heap. Bound for the whole frame, shared with ImGui.
  auto GetSRVHeap() -> DXDescriptorHeap &;
  auto GetRTVHeap() -> DXDescriptorHeap &;
  auto GetDSVHeap() -> DXDescriptorHeap &;
  auto GetBackBuffer() const -> ID3D12Resource *;
  auto GetBackBufferRTV() const -> D3D12_CPU_DESCRIPTOR_HANDLE;
  auto GetBackBufferFormat() const -> DXGI_FORMAT;
  auto GetWidth() const -> int;
  auto GetHeight() const -> int;
  /// @brief Blocks until the GPU has drained every submitted frame. Required before releasing resources.
  auto WaitForGPU() -> void;
  /// @brief Closes, submits and waits on the current command list, then reopens it.
  ///
  /// Used by one-off work that has to complete before the frame continues, such as staging an
  /// upload heap into a default-heap resource. Stalls the pipeline, so it is for load time only.
  auto FlushCommandList() -> void;
  /// @brief Keeps a resource alive until the GPU has finished the frame being recorded right now.
  ///
  /// An upload staging buffer is read by a copy the GPU has not run yet, so releasing it when the
  /// recording function returns frees memory out from under that copy. Historically the only way
  /// to avoid that was to flush and wait before returning, which is why every upload used to cost
  /// a full pipeline drain. Handing the buffer here instead defers the release to the fence the
  /// frame already signals, so the upload path can batch without stalling at all.
  ///
  /// Stamped with the value `Present` is about to signal, so a resource retired at any point
  /// during recording is held until the whole of that frame's work has retired.
  auto RetireResource(ComPtr<ID3D12Resource>) -> void;
  /// @brief Releases every retired resource whose frame the GPU has finished with.
  ///
  /// Runs on its own at the top of each frame. Callers that need the memory back sooner, such as
  /// one that has just flushed to stay inside an upload budget, can ask for the sweep directly.
  auto ReleaseRetiredResources() -> void;
  /// @brief How many bytes of system-memory heap DXGI says this adapter can use before it evicts.
  ///
  /// Upload heaps are allocated out of the non-local segment on a discrete adapter, so this is the
  /// ceiling an upload budget has to stay under. Zero when the adapter predates `IDXGIAdapter3` or
  /// refuses the query, which the caller should read as "no information", not as "no memory".
  auto GetUploadHeapBudget() const -> uint64_t;
private:
  static constexpr DXGI_FORMAT BACK_BUFFER_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
  ComPtr<IDXGIFactory6> factory;
  ComPtr<IDXGIAdapter1> adapter;
  ComPtr<ID3D12Device> device;
  ComPtr<ID3D12Device5> device5;
  ComPtr<ID3D12CommandQueue> commandQueue;
  ComPtr<IDXGISwapChain3> swapChain;
  ComPtr<ID3D12GraphicsCommandList> commandList;
  ComPtr<ID3D12GraphicsCommandList4> commandList4;
  DXCapabilities capabilities;
  ComPtr<ID3D12Fence> fence;
  std::array<ComPtr<ID3D12CommandAllocator>, DX_FRAME_COUNT> commandAllocators;
  std::array<ComPtr<ID3D12Resource>, DX_FRAME_COUNT> backBuffers;
  std::array<uint32_t, DX_FRAME_COUNT> backBufferRTVs{};
  std::array<uint64_t, DX_FRAME_COUNT> fenceValues{};
  /// @brief Resources awaiting the fence value they were retired against, oldest first.
  std::vector<std::pair<uint64_t, ComPtr<ID3D12Resource>>> pendingReleases;
  DXDescriptorHeap srvHeap;
  DXDescriptorHeap rtvHeap;
  DXDescriptorHeap dsvHeap;
  HANDLE fenceEvent{};
  HWND hwnd{};
  uint64_t nextFenceValue{1};
  uint32_t frameIndex{};
  int width{};
  int height{};
  bool frameOpen{};
  bool vsync{true};
  bool tearingSupported{};
  auto CreateDeviceAndQueue() -> bool;
  auto CreateSwapChain() -> bool;
  auto CreateFrameResources() -> bool;
  auto AcquireBackBuffers() -> bool;
  auto ReleaseBackBuffers() -> void;
  auto MoveToNextFrame() -> void;
};
} // namespace kuki
#endif
