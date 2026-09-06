#include <dx_shader_compiler.hpp>
#ifdef KUKI_HAS_DIRECTX
#include <format>
#include <shader_definitions.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
namespace kuki {
namespace {
auto ToWide(const std::string &text) -> std::wstring {
  return std::wstring(text.begin(), text.end());
}
auto ReadLog(IDxcResult *result) -> std::string {
  ComPtr<IDxcBlobUtf8> errors;
  if (FAILED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr)) || !errors || errors->GetStringLength() == 0)
    return {};
  auto log = std::string(errors->GetStringPointer(), errors->GetStringLength());
  while (!log.empty() && (log.back() == '\n' || log.back() == '\r'))
    log.pop_back();
  return log;
}
} // namespace
DXShaderCompiler::DXShaderCompiler() = default;
DXShaderCompiler::~DXShaderCompiler() {
  Shutdown();
}
auto DXShaderCompiler::Initialize() -> bool {
  if (compiler)
    return true;
  library = LoadLibraryW(L"dxcompiler.dll");
  if (!library) {
    spdlog::error("[DX12] dxcompiler.dll could not be loaded; no shader can be compiled");
    return false;
  }
  const auto createInstance = reinterpret_cast<DxcCreateInstanceProc>(reinterpret_cast<void *>(GetProcAddress(library, "DxcCreateInstance")));
  if (!createInstance) {
    spdlog::error("[DX12] dxcompiler.dll exports no DxcCreateInstance");
    Shutdown();
    return false;
  }
  if (DXFailed(createInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)), "DxcCreateInstance(DxcUtils)") || DXFailed(createInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)), "DxcCreateInstance(DxcCompiler)")) {
    Shutdown();
    return false;
  }
  if (DXFailed(utils->CreateDefaultIncludeHandler(&includeHandler), "CreateDefaultIncludeHandler")) {
    Shutdown();
    return false;
  }
  ComPtr<IDxcVersionInfo> version;
  if (SUCCEEDED(compiler.As(&version))) {
    UINT32 major{};
    UINT32 minor{};
    version->GetVersion(&major, &minor);
    spdlog::info("[DX12] HLSL compiler {}.{} loaded", major, minor);
  }
  return true;
}
auto DXShaderCompiler::Shutdown() -> void {
  includeHandler.Reset();
  compiler.Reset();
  utils.Reset();
  if (library) {
    FreeLibrary(library);
    library = nullptr;
  }
}
auto DXShaderCompiler::Compile(const std::string_view source, const std::string &entryPoint, const std::string &target, const std::string &name) -> ComPtr<IDxcBlob> {
  if (!Initialize())
    return nullptr;
  const auto sourceName = ToWide(name + ".hlsl");
  const auto wideEntryPoint = ToWide(entryPoint);
  const auto wideTarget = ToWide(target);
  // Held alive for as long as the argument vector points into them. `arguments` stores raw
  // pointers, so every wide string it names has to outlive the call to `Compile` below.
  std::vector<std::wstring> definitions;
  definitions.reserve(std::size(SHADER_DEFINITIONS));
  for (const auto &definition : SHADER_DEFINITIONS)
    definitions.push_back(ToWide(std::format("{}={}u", definition.name, definition.value)));
  std::vector<LPCWSTR> arguments{sourceName.c_str(), L"-E", wideEntryPoint.c_str(), L"-T", wideTarget.c_str(), L"-Ges"};
  for (const auto &definition : definitions)
    arguments.insert(arguments.end(), {L"-D", definition.c_str()});
#ifndef NDEBUG
  arguments.insert(arguments.end(), {L"-Zi", L"-Od", L"-Qembed_debug"});
#else
  arguments.push_back(L"-O3");
#endif
  DxcBuffer buffer{};
  buffer.Ptr = source.data();
  buffer.Size = source.size();
  buffer.Encoding = DXC_CP_UTF8;
  ComPtr<IDxcResult> result;
  if (DXFailed(compiler->Compile(&buffer, arguments.data(), static_cast<UINT32>(arguments.size()), includeHandler.Get(), IID_PPV_ARGS(&result)), std::format("compiling {} ({})", name, entryPoint)))
    return nullptr;
  HRESULT status{};
  const auto log = ReadLog(result.Get());
  if (FAILED(result->GetStatus(&status)) || FAILED(status)) {
    spdlog::error("[DX12] Failed to compile {} ({}): {}", name, entryPoint, log.empty() ? "no compiler output" : log);
    return nullptr;
  }
  if (!log.empty())
    spdlog::warn("[DX12] {} ({}): {}", name, entryPoint, log);
  ComPtr<IDxcBlob> bytecode;
  if (DXFailed(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&bytecode), nullptr), std::format("reading {} ({}) bytecode", name, entryPoint)) || !bytecode)
    return nullptr;
  return bytecode;
}
} // namespace kuki
#endif
