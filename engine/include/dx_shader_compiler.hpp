#pragma once
#ifdef KUKI_HAS_DIRECTX
#include <dx_common.hpp>
#include <dxcapi.h>
#include <kuki_engine_export.h>
#include <string>
#include <string_view>
namespace kuki {
/// @brief Compiles HLSL to DXIL with the standalone shader compiler.
///
/// The compiler that ships in `d3dcompiler.dll` stops at Shader Model 5.1, which predates every
/// feature the raytraced passes need: inline ray queries, and addressing the descriptor heap
/// directly rather than through a root signature table. Those live in Shader Model 6, which only
/// this compiler emits.
///
/// `dxcompiler.dll` is loaded by name at runtime rather than linked, so a missing compiler is a
/// logged failure at the first shader build instead of a process that will not start at all. It
/// signs its output through `dxil.dll`, which it finds beside itself; unsigned bytecode is refused
/// by the runtime unless the machine is in developer mode. Both are staged next to the executable
/// by the build.
class KUKI_ENGINE_API DXShaderCompiler final {
public:
  DXShaderCompiler();
  ~DXShaderCompiler();
  DXShaderCompiler(const DXShaderCompiler &) = delete;
  auto operator=(const DXShaderCompiler &) -> DXShaderCompiler & = delete;
  /// @brief Loads the compiler and creates its interfaces. Safe to call repeatedly.
  /// @return False when `dxcompiler.dll` is absent or too old to provide the interfaces.
  auto Initialize() -> bool;
  auto Shutdown() -> void;
  /// @brief Compiles one entry point out of an HLSL source string.
  ///
  /// Initialises the compiler on first use, so callers need not sequence that themselves.
  ///
  /// @param source The whole shader file; one file usually holds several entry points.
  /// @param entryPoint Function to compile, naming which of those entry points is wanted.
  /// @param target Profile in `xs_6_n` form, which also fixes the minimum Shader Model required.
  /// @param name Stem used to name the source in diagnostics, so an error points at a real file.
  /// @return The compiled bytecode, or null on failure, with the compiler log written to the error log.
  auto Compile(const std::string_view source, const std::string &entryPoint, const std::string &target, const std::string &name) -> ComPtr<IDxcBlob>;
private:
  HMODULE library{};
  ComPtr<IDxcUtils> utils;
  ComPtr<IDxcCompiler3> compiler;
  ComPtr<IDxcIncludeHandler> includeHandler;
};
} // namespace kuki
#endif
