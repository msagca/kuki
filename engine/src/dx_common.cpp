#include <dx_common.hpp>
#ifdef KUKI_HAS_DIRECTX
#include <format>
#include <spdlog/spdlog.h>
#include <string>
namespace kuki {
auto DXResultToString(const HRESULT result) -> std::string {
  char *buffer{};
  const auto length = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, static_cast<DWORD>(result), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<char *>(&buffer), 0, nullptr);
  std::string message;
  if (length > 0 && buffer) {
    message.assign(buffer, length);
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r'))
      message.pop_back();
  }
  if (buffer)
    LocalFree(buffer);
  return std::format("0x{:08X}{}{}", static_cast<uint32_t>(result), message.empty() ? "" : ": ", message);
}
auto DXFailed(const HRESULT result, const std::string &context) -> bool {
  if (SUCCEEDED(result))
    return false;
  spdlog::error("[DX12] {} failed, {}", context, DXResultToString(result));
  return true;
}
auto DXCapabilities::SupportsInlineRaytracing() const -> bool {
  return raytracingTier >= D3D12_RAYTRACING_TIER_1_1 && shaderModel >= D3D_SHADER_MODEL_6_5;
}
auto DXCapabilities::SupportsBindless() const -> bool {
  return bindingTier >= D3D12_RESOURCE_BINDING_TIER_3 && shaderModel >= D3D_SHADER_MODEL_6_0;
}
auto DXCapabilities::SupportsDynamicResources() const -> bool {
  return shaderModel >= D3D_SHADER_MODEL_6_6;
}
auto DXShaderModelToString(const D3D_SHADER_MODEL model) -> std::string {
  return std::format("{}_{}", static_cast<uint32_t>(model) >> 4, static_cast<uint32_t>(model) & 0xF);
}
auto SelectDXDevice(IDXGIFactory6 *factory, ComPtr<IDXGIAdapter1> &adapter, ComPtr<ID3D12Device> &device) -> bool {
  adapter.Reset();
  device.Reset();
  if (!factory)
    return false;
  for (uint32_t i = 0; factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
    DXGI_ADAPTER_DESC1 desc{};
    adapter->GetDesc1(&desc);
    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
      continue;
    if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
      return true;
    adapter.Reset();
  }
  device.Reset();
  return false;
}
auto DXDeviceAvailable() -> bool {
  // Asked once and remembered. A device is created and dropped to find the answer, so this is not
  // free, and the caller is a menu that redraws every frame.
  static const auto available = [] {
    ComPtr<IDXGIFactory6> factory;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))))
      return false;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    const auto found = SelectDXDevice(factory.Get(), adapter, device);
    if (!found)
      spdlog::info("[DX12] no adapter provides a feature level 11_0 device, so OpenGL is the default here.");
    return found;
  }();
  return available;
}
auto QueryDXCapabilities(ID3D12Device *device) -> DXCapabilities {
  DXCapabilities capabilities;
  if (!device)
    return capabilities;
  constexpr D3D_SHADER_MODEL CANDIDATES[]{D3D_SHADER_MODEL_6_8, D3D_SHADER_MODEL_6_7, D3D_SHADER_MODEL_6_6, D3D_SHADER_MODEL_6_5, D3D_SHADER_MODEL_6_4, D3D_SHADER_MODEL_6_3, D3D_SHADER_MODEL_6_2, D3D_SHADER_MODEL_6_1, D3D_SHADER_MODEL_6_0};
  for (const auto candidate : CANDIDATES) {
    D3D12_FEATURE_DATA_SHADER_MODEL query{candidate};
    if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &query, sizeof(query)))) {
      capabilities.shaderModel = query.HighestShaderModel;
      break;
    }
  }
  D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5{};
  if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5))))
    capabilities.raytracingTier = options5.RaytracingTier;
  D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
  if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options))))
    capabilities.bindingTier = options.ResourceBindingTier;
  return capabilities;
}
} // namespace kuki
#endif
