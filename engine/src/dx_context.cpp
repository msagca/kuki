#define GLFW_INCLUDE_NONE
#include <dx_context.hpp>
#ifdef KUKI_HAS_DIRECTX
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dx_common.hpp>
#include <spdlog/spdlog.h>
#include <string>
namespace kuki {
namespace {
constexpr uint32_t SRV_HEAP_CAPACITY = 4096;
constexpr uint32_t RTV_HEAP_CAPACITY = 256;
constexpr uint32_t DSV_HEAP_CAPACITY = 128;
auto DescribeAdapter(IDXGIAdapter1 *adapter) -> std::string {
  DXGI_ADAPTER_DESC1 desc{};
  if (!adapter || FAILED(adapter->GetDesc1(&desc)))
    return "unknown adapter";
  std::string name;
  for (auto i = 0; i < 128 && desc.Description[i]; ++i)
    name.push_back(static_cast<char>(desc.Description[i]));
  return std::format("{} ({} MB dedicated)", name, desc.DedicatedVideoMemory / (1024 * 1024));
}
auto DescribeRaytracingTier(const D3D12_RAYTRACING_TIER tier) -> std::string {
  if (tier >= D3D12_RAYTRACING_TIER_1_1)
    return "1.1";
  if (tier >= D3D12_RAYTRACING_TIER_1_0)
    return "1.0";
  return "unsupported";
}
} // namespace
DXContext::~DXContext() {
  Shutdown();
}
auto DXContext::ApplyWindowHints() const -> void {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}
auto DXContext::Initialize(GLFWwindow *window) -> bool {
  this->window = window;
  hwnd = glfwGetWin32Window(window);
  if (!hwnd) {
    spdlog::error("[DX12] Failed to obtain the native window handle");
    return false;
  }
  glfwGetFramebufferSize(window, &width, &height);
  if (!CreateDeviceAndQueue())
    return false;
  if (!CreateSwapChain())
    return false;
  if (!CreateFrameResources())
    return false;
  spdlog::info("[DX12] Initialised on {}", DescribeAdapter(adapter.Get()));
  spdlog::info("[DX12] Shader model {}, raytracing tier {}, resource binding tier {}", DXShaderModelToString(capabilities.shaderModel), DescribeRaytracingTier(capabilities.raytracingTier), static_cast<int>(capabilities.bindingTier));
  spdlog::info("[DX12] Bindless {}, inline raytracing {}", capabilities.SupportsBindless() ? "available" : "unavailable", capabilities.SupportsInlineRaytracing() ? "available" : "unavailable");
  if (!capabilities.SupportsBindless() || !capabilities.SupportsInlineRaytracing())
    spdlog::warn("[DX12] The passes that trace rays need both; they will be skipped");
  spdlog::info("[DX12] Tearing {}", tearingSupported ? "supported, and used when vsync is off" : "UNSUPPORTED - the compositor will pace presents to the refresh rate");
  return true;
}
auto DXContext::CreateDeviceAndQueue() -> bool {
  uint32_t factoryFlags = 0;
  // Before anything else here: Device Removed Extended Data cannot be switched on once a device
  // exists, so a run that did not ask for it at this point cannot be asked about it later.
  DXEnableDeviceRemovedDiagnostics();
#ifndef NDEBUG
  ComPtr<ID3D12Debug> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
    debugController->EnableDebugLayer();
    factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    spdlog::info("[DX12] Debug layer enabled");
  }
#endif
  if (DXFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2"))
    return false;
  BOOL allowTearing = FALSE;
  if (SUCCEEDED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
    tearingSupported = allowTearing == TRUE;
  // The same choice `DXDeviceAvailable` made before the backend was picked, so what runs is what
  // was probed rather than a second opinion about the same machine.
  if (!SelectDXDevice(factory.Get(), adapter, device)) {
    spdlog::error("[DX12] No adapter supports feature level 11_0");
    return false;
  }
  capabilities = QueryDXCapabilities(device.Get());
  if (capabilities.raytracingTier > D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
    device.As(&device5);
#ifndef NDEBUG
  ComPtr<ID3D12InfoQueue> infoQueue;
  if (SUCCEEDED(device.As(&infoQueue))) {
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
  }
#endif
  D3D12_COMMAND_QUEUE_DESC queueDesc{};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  if (DXFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)), "CreateCommandQueue"))
    return false;
  return true;
}
auto DXContext::CreateSwapChain() -> bool {
  DXGI_SWAP_CHAIN_DESC1 desc{};
  desc.BufferCount = DX_FRAME_COUNT;
  desc.Width = static_cast<UINT>(width);
  desc.Height = static_cast<UINT>(height);
  desc.Format = BACK_BUFFER_FORMAT;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  desc.SampleDesc.Count = 1;
  desc.Flags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u;
  ComPtr<IDXGISwapChain1> swapChain1;
  if (DXFailed(factory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &desc, nullptr, nullptr, &swapChain1), "CreateSwapChainForHwnd"))
    return false;
  factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
  if (DXFailed(swapChain1.As(&swapChain), "IDXGISwapChain3 query"))
    return false;
  frameIndex = swapChain->GetCurrentBackBufferIndex();
  return true;
}
auto DXContext::CreateFrameResources() -> bool {
  if (!rtvHeap.Initialize(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, RTV_HEAP_CAPACITY))
    return false;
  if (!dsvHeap.Initialize(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, DSV_HEAP_CAPACITY))
    return false;
  if (!srvHeap.Initialize(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, SRV_HEAP_CAPACITY, true))
    return false;
  for (uint32_t i = 0; i < DX_FRAME_COUNT; ++i) {
    backBufferRTVs[i] = rtvHeap.Allocate();
    if (DXFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[i])), "CreateCommandAllocator"))
      return false;
  }
  if (!AcquireBackBuffers())
    return false;
  if (DXFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[frameIndex].Get(), nullptr, IID_PPV_ARGS(&commandList)), "CreateCommandList"))
    return false;
  if (device5)
    commandList.As(&commandList4);
  commandList->Close();
  if (DXFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "CreateFence"))
    return false;
  fenceValues.fill(0);
  fenceEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
  if (!fenceEvent) {
    spdlog::error("[DX12] Failed to create the fence event");
    return false;
  }
  return true;
}
auto DXContext::AcquireBackBuffers() -> bool {
  for (uint32_t i = 0; i < DX_FRAME_COUNT; ++i) {
    if (DXFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i])), "SwapChain::GetBuffer"))
      return false;
    device->CreateRenderTargetView(backBuffers[i].Get(), nullptr, rtvHeap.GetCPUHandle(backBufferRTVs[i]));
  }
  return true;
}
auto DXContext::ReleaseBackBuffers() -> void {
  for (auto &buffer : backBuffers)
    buffer.Reset();
}
auto DXContext::BeginFrame() -> void {
  if (!device || frameOpen || immediateOpen)
    return;
  if (fence->GetCompletedValue() < fenceValues[frameIndex]) {
    if (DXFailed(fence->SetEventOnCompletion(fenceValues[frameIndex], fenceEvent), "SetEventOnCompletion"))
      return;
    WaitForSingleObject(fenceEvent, INFINITE);
  }
  ReleaseRetiredResources();
  if (DXFailed(commandAllocators[frameIndex]->Reset(), "CommandAllocator::Reset"))
    return;
  if (DXFailed(commandList->Reset(commandAllocators[frameIndex].Get(), nullptr), "CommandList::Reset"))
    return;
  frameOpen = true;
  auto *heap = srvHeap.Get();
  commandList->SetDescriptorHeaps(1, &heap);
  const auto toRenderTarget = CD3DX12_RESOURCE_BARRIER::Transition(backBuffers[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
  commandList->ResourceBarrier(1, &toRenderTarget);
  const auto rtv = GetBackBufferRTV();
  constexpr float CLEAR_COLOR[4]{.1f, .1f, .1f, 1.f};
  commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
  commandList->ClearRenderTargetView(rtv, CLEAR_COLOR, 0, nullptr);
  const auto viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<float>(width), static_cast<float>(height));
  const auto scissor = CD3DX12_RECT(0, 0, static_cast<LONG>(width), static_cast<LONG>(height));
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
}
auto DXContext::Present() -> void {
  if (!device || !frameOpen)
    return;
  const auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(backBuffers[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  commandList->ResourceBarrier(1, &toPresent);
  if (DXFailed(commandList->Close(), "CommandList::Close"))
    return;
  ID3D12CommandList *lists[]{commandList.Get()};
  commandQueue->ExecuteCommandLists(1, lists);
  frameOpen = false;
  const auto flags = tearingSupported && !vsync ? DXGI_PRESENT_ALLOW_TEARING : 0u;
  if (const auto presented = swapChain->Present(vsync ? 1u : 0u, flags); DXFailed(presented, "SwapChain::Present")) {
    // Present is usually the first call to notice, because it is the first one that waits on the
    // GPU rather than merely recording for it. Everything after this fails with the same code, so
    // the reason is asked for here and once.
    if (DXDeviceIsGone(presented))
      DXReportDeviceRemoved(device.Get(), "Present");
    return;
  }
  MoveToNextFrame();
}
auto DXContext::MoveToNextFrame() -> void {
  const auto signalled = nextFenceValue++;
  if (DXFailed(commandQueue->Signal(fence.Get(), signalled), "CommandQueue::Signal"))
    return;
  fenceValues[frameIndex] = signalled;
  frameIndex = swapChain->GetCurrentBackBufferIndex();
}
auto DXContext::BeginImmediate() -> ID3D12GraphicsCommandList * {
  if (!device || frameOpen || immediateOpen)
    return nullptr;
  // The allocator belongs to the frame with this index, whose work may still be running: resetting
  // it under the GPU is what this wait is for. `BeginFrame` waits on that frame's fence for the
  // same reason; here there is no frame to take the value from, so the wait is the whole queue.
  WaitForGPU();
  if (DXFailed(commandAllocators[frameIndex]->Reset(), "CommandAllocator::Reset"))
    return nullptr;
  if (DXFailed(commandList->Reset(commandAllocators[frameIndex].Get(), nullptr), "CommandList::Reset"))
    return nullptr;
  immediateOpen = true;
  auto *heap = srvHeap.Get();
  commandList->SetDescriptorHeaps(1, &heap);
  return commandList.Get();
}
auto DXContext::EndImmediate() -> void {
  if (!immediateOpen)
    return;
  immediateOpen = false;
  // Closed rather than left open, because `BeginFrame` resets both the allocator and the list, and
  // neither reset is legal on a list still recording.
  DXFailed(commandList->Close(), "CommandList::Close");
}
auto DXContext::FlushCommandList() -> void {
  if (!device || (!frameOpen && !immediateOpen))
    return;
  if (DXFailed(commandList->Close(), "CommandList::Close"))
    return;
  ID3D12CommandList *lists[]{commandList.Get()};
  commandQueue->ExecuteCommandLists(1, lists);
  WaitForGPU();
  if (DXFailed(commandAllocators[frameIndex]->Reset(), "CommandAllocator::Reset"))
    return;
  if (DXFailed(commandList->Reset(commandAllocators[frameIndex].Get(), nullptr), "CommandList::Reset"))
    return;
  auto *heap = srvHeap.Get();
  commandList->SetDescriptorHeaps(1, &heap);
}
auto DXContext::RetireResource(ComPtr<ID3D12Resource> resource) -> void {
  if (!resource)
    return;
  pendingReleases.emplace_back(nextFenceValue, std::move(resource));
}
auto DXContext::ReleaseRetiredResources() -> void {
  if (pendingReleases.empty())
    return;
  const auto completed = fence ? fence->GetCompletedValue() : UINT64_MAX;
  std::erase_if(pendingReleases, [completed](const auto &entry) { return entry.first <= completed; });
}
auto DXContext::GetUploadHeapBudget() const -> uint64_t {
  ComPtr<IDXGIAdapter3> adapter3;
  if (!adapter || FAILED(adapter.As(&adapter3)))
    return 0;
  DXGI_QUERY_VIDEO_MEMORY_INFO info{};
  if (FAILED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &info)))
    return 0;
  return info.Budget;
}
auto DXContext::WaitForGPU() -> void {
  if (!commandQueue || !fence || !fenceEvent)
    return;
  const auto signalled = nextFenceValue++;
  if (DXFailed(commandQueue->Signal(fence.Get(), signalled), "CommandQueue::Signal"))
    return;
  if (fence->GetCompletedValue() >= signalled)
    return;
  if (DXFailed(fence->SetEventOnCompletion(signalled, fenceEvent), "SetEventOnCompletion"))
    return;
  WaitForSingleObject(fenceEvent, INFINITE);
}
auto DXContext::SetVSync(const bool enabled) -> void {
  vsync = enabled;
  spdlog::info("[DX12] Vsync {} (sync interval {})", enabled ? "on" : "off", enabled ? 1 : 0);
}
auto DXContext::OnResize(const int newWidth, const int newHeight) -> void {
  if (!swapChain || newWidth <= 0 || newHeight <= 0)
    return;
  if (newWidth == width && newHeight == height)
    return;
  WaitForGPU();
  if (frameOpen || immediateOpen) {
    commandList->Close();
    frameOpen = false;
    immediateOpen = false;
  }
  ReleaseBackBuffers();
  width = newWidth;
  height = newHeight;
  const auto flags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u;
  if (DXFailed(swapChain->ResizeBuffers(DX_FRAME_COUNT, static_cast<UINT>(width), static_cast<UINT>(height), BACK_BUFFER_FORMAT, flags), "SwapChain::ResizeBuffers"))
    return;
  frameIndex = swapChain->GetCurrentBackBufferIndex();
  fenceValues.fill(0);
  AcquireBackBuffers();
}
auto DXContext::Shutdown() -> void {
  if (!device)
    return;
  WaitForGPU();
  pendingReleases.clear();
  if (fenceEvent) {
    CloseHandle(fenceEvent);
    fenceEvent = nullptr;
  }
  ReleaseBackBuffers();
  srvHeap.Shutdown();
  rtvHeap.Shutdown();
  dsvHeap.Shutdown();
  commandList4.Reset();
  commandList.Reset();
  for (auto &allocator : commandAllocators)
    allocator.Reset();
  fence.Reset();
  swapChain.Reset();
  commandQueue.Reset();
  device5.Reset();
  device.Reset();
  adapter.Reset();
  factory.Reset();
  window = nullptr;
  hwnd = nullptr;
}
auto DXContext::GetDevice() const -> ID3D12Device * {
  return device.Get();
}
auto DXContext::GetDevice5() const -> ID3D12Device5 * {
  return device5.Get();
}
auto DXContext::GetCapabilities() const -> const DXCapabilities & {
  return capabilities;
}
auto DXContext::GetCommandList() const -> ID3D12GraphicsCommandList * {
  return frameOpen || immediateOpen ? commandList.Get() : nullptr;
}
auto DXContext::GetCommandList4() const -> ID3D12GraphicsCommandList4 * {
  return frameOpen || immediateOpen ? commandList4.Get() : nullptr;
}
auto DXContext::GetCommandQueue() const -> ID3D12CommandQueue * {
  return commandQueue.Get();
}
auto DXContext::GetFrameIndex() const -> uint32_t {
  return frameIndex;
}
auto DXContext::GetSRVHeap() -> DXDescriptorHeap & {
  return srvHeap;
}
auto DXContext::GetRTVHeap() -> DXDescriptorHeap & {
  return rtvHeap;
}
auto DXContext::GetDSVHeap() -> DXDescriptorHeap & {
  return dsvHeap;
}
auto DXContext::GetBackBuffer() const -> ID3D12Resource * {
  return backBuffers[frameIndex].Get();
}
auto DXContext::GetBackBufferRTV() const -> D3D12_CPU_DESCRIPTOR_HANDLE {
  return rtvHeap.GetCPUHandle(backBufferRTVs[frameIndex]);
}
auto DXContext::GetBackBufferFormat() const -> DXGI_FORMAT {
  return BACK_BUFFER_FORMAT;
}
auto DXContext::GetWidth() const -> int {
  return width;
}
auto DXContext::GetHeight() const -> int {
  return height;
}
} // namespace kuki
#endif
