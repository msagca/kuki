// Compiles the engine's HLSL to DXIL while the engine is built, and writes it out as a source file
// the engine compiles in: `KukiShaderCompiler`, run by the custom command in `engine/CMakeLists.txt`.
//
// Run as `KukiShaderCompiler --output <file.cpp> [--debug --pdb <dir>] <shader.hlsl>...`.
//
// The shaders used to be embedded as text and compiled by `dxcompiler.dll` the first time each
// pipeline was asked for. That was most of the first frame of every game -- seconds of it, the
// scene pass alone 680-990 ms -- and a shader that did not compile showed only when run. Compiled
// here, a mistake fails the build with the compiler's message, and nothing compiles HLSL at run
// time, so the compiler is not shipped either. What the driver does with the DXIL when a pipeline
// is created (turning it into the GPU's own code) still happens on the player's machine, as it must:
// that code belongs to one GPU and one driver version.
//
// The tool is the only place `dxcompiler.dll` is loaded. It is copied beside the tool with
// `dxil.dll`, which it loads by bare name from the directory it is in; `dxil.dll` signs the output,
// and the runtime refuses unsigned DXIL outside developer mode, so a missing signer fails here
// rather than on a machine that is not in developer mode.
//
// Entry points are found rather than listed: every function at the start of a line whose name is a
// stage's prefix and a capital (`VSMain`, `PSOcclusionBlur`, `CSTrace`) is one, and nothing else in
// `engine/src/shader/` is named that way. A list kept beside the shaders would be one more place to
// forget a new entry point in; the name is already the convention every shader follows.
#include <windows.h>
// Apart from the rest, so sorting cannot put `dxcapi.h` above `windows.h`, which it assumes came first.
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <dxcapi.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <regex>
#include <shader_definitions.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>
#include <wrl/client.h>
namespace {
using Microsoft::WRL::ComPtr;
struct EntryPoint {
  std::filesystem::path file;
  std::string stem;
  std::string name;
  std::string target;
  std::vector<unsigned char> bytecode;
};
struct Options {
  std::filesystem::path output;
  std::filesystem::path pdbDirectory;
  bool debug{};
  std::vector<std::filesystem::path> shaders;
};
auto ToWide(const std::string &text) -> std::wstring {
  return {text.begin(), text.end()};
}
auto ReadFile(const std::filesystem::path &path, std::string &content) -> bool {
  std::ifstream stream(path, std::ios::binary);
  if (!stream)
    return false;
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  content = buffer.str();
  return true;
}
// Shader Model 6.5 for a file that traces rays inline, 6.0 for the rest. `RayQuery` is what needs
// 6.5, so it is what decides: compiling every file at 6.5 would refuse a GPU without it the passes
// that never trace a ray, where today the raytraced ones are the only ones to go.
auto ShaderModel(const std::string &source) -> const char * {
  return source.find("RayQuery<") != std::string::npos ? "6_5" : "6_0";
}
auto FindEntryPoints(const std::filesystem::path &file, const std::string &source, std::vector<EntryPoint> &entries) -> void {
  // A return type, then the name. The return type keeps a call or a declaration inside a body from
  // matching, since a body is indented and its statements do not begin with a type at column one.
  static const std::regex DECLARATION(R"(^[A-Za-z_][A-Za-z0-9_<>]*\s+((VS|PS|CS)[A-Z][A-Za-z0-9_]*)\s*\()", std::regex::multiline);
  const auto *model = ShaderModel(source);
  for (auto it = std::sregex_iterator(source.begin(), source.end(), DECLARATION); it != std::sregex_iterator(); ++it) {
    auto stage = (*it)[2].str();
    std::ranges::transform(stage, stage.begin(), [](const char c) { return static_cast<char>(c - 'A' + 'a'); });
    entries.push_back({file, file.stem().string(), (*it)[1].str(), std::format("{}_{}", stage, model), {}});
  }
}
// The container's digest, which `dxil.dll` fills in when it signs. All zeroes is unsigned DXIL, which
// every machine not in developer mode refuses at pipeline creation.
auto IsSigned(const std::vector<unsigned char> &bytecode) -> bool {
  constexpr size_t DIGEST_OFFSET = 4;
  constexpr size_t DIGEST_SIZE = 16;
  if (bytecode.size() < DIGEST_OFFSET + DIGEST_SIZE)
    return false;
  return std::any_of(bytecode.begin() + DIGEST_OFFSET, bytecode.begin() + DIGEST_OFFSET + DIGEST_SIZE, [](const unsigned char byte) { return byte != 0; });
}
class Compiler {
public:
  explicit Compiler(DxcCreateInstanceProc create) {
    if (FAILED(create(CLSID_DxcUtils, IID_PPV_ARGS(&utils))) || FAILED(create(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler))) || FAILED(utils->CreateDefaultIncludeHandler(&includeHandler)))
      compiler.Reset();
  }
  [[nodiscard]] auto IsReady() const -> bool {
    return compiler != nullptr;
  }
  // Fills the entry's bytecode, or returns the compiler's message.
  auto Compile(EntryPoint &entry, const std::string &source, const Options &options) -> std::string {
    // The full path names the source, so an error points at a file an editor can open.
    const auto sourceName = entry.file.wstring();
    const auto name = ToWide(entry.name);
    const auto target = ToWide(entry.target);
    std::vector<std::wstring> definitions;
    for (const auto &definition : kuki::SHADER_DEFINITIONS)
      definitions.push_back(ToWide(std::format("{}={}u", definition.name, definition.value)));
    std::vector<LPCWSTR> arguments{sourceName.c_str(), L"-E", name.c_str(), L"-T", target.c_str(), L"-Ges"};
    for (const auto &definition : definitions)
      arguments.insert(arguments.end(), {L"-D", definition.c_str()});
    // Debug keeps the debug information out of the bytecode, in a PDB of its own that PIX finds by
    // the name the bytecode carries. Embedded, it made the DXIL seven times the size -- 3.3 MB
    // against 0.47 MB -- all of it compiled into the engine for a debugger that reads it rarely.
    const auto pdbDirectory = (options.pdbDirectory / "").wstring();
    if (options.debug)
      arguments.insert(arguments.end(), {L"-Zi", L"-Od", L"-Qstrip_debug", L"-Fd", pdbDirectory.c_str()});
    else
      arguments.push_back(L"-O3");
    DxcBuffer buffer{source.data(), source.size(), DXC_CP_UTF8};
    ComPtr<IDxcResult> result;
    if (FAILED(compiler->Compile(&buffer, arguments.data(), static_cast<UINT32>(arguments.size()), includeHandler.Get(), IID_PPV_ARGS(&result))))
      return "the compiler could not be run";
    ComPtr<IDxcBlobUtf8> errors;
    std::string log;
    if (SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr)) && errors && errors->GetStringLength() > 0)
      log.assign(errors->GetStringPointer(), errors->GetStringLength());
    HRESULT status{};
    if (FAILED(result->GetStatus(&status)) || FAILED(status))
      return log.empty() ? std::string("no compiler output") : log;
    ComPtr<IDxcBlob> bytecode;
    if (FAILED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&bytecode), nullptr)) || !bytecode)
      return "no bytecode";
    const auto *begin = static_cast<const unsigned char *>(bytecode->GetBufferPointer());
    entry.bytecode.assign(begin, begin + bytecode->GetBufferSize());
    if (!IsSigned(entry.bytecode))
      return "the bytecode is not signed: dxil.dll is missing beside the tool, and unsigned DXIL is refused outside developer mode";
    if (options.debug) {
      ComPtr<IDxcBlob> pdb;
      ComPtr<IDxcBlobUtf16> pdbName;
      if (SUCCEEDED(result->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pdb), &pdbName)) && pdb && pdbName) {
        // The name is the file's alone (the hash of the shader), whatever directory `-Fd` named,
        // so it is put in the directory here -- given as it comes, it lands in the working one.
        std::ofstream stream(options.pdbDirectory / std::filesystem::path(pdbName->GetStringPointer()).filename(), std::ios::binary);
        stream.write(static_cast<const char *>(pdb->GetBufferPointer()), static_cast<std::streamsize>(pdb->GetBufferSize()));
      }
    }
    // A warning is printed and does not fail the build, as the C++ compiler's do not.
    if (!log.empty())
      std::fprintf(stderr, "%s\n", log.c_str());
    return {};
  }
private:
  ComPtr<IDxcUtils> utils;
  ComPtr<IDxcCompiler3> compiler;
  ComPtr<IDxcIncludeHandler> includeHandler;
};
auto ParseOptions(const int argc, char **argv, Options &options) -> bool {
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--output" && i + 1 < argc)
      options.output = argv[++i];
    else if (argument == "--pdb" && i + 1 < argc)
      options.pdbDirectory = argv[++i];
    else if (argument == "--debug")
      options.debug = true;
    else
      options.shaders.emplace_back(argument);
  }
  return !options.output.empty() && !options.shaders.empty() && (!options.debug || !options.pdbDirectory.empty());
}
// One array per entry point, and a table the engine looks them up in by file and name.
auto WriteSource(const Options &options, const std::vector<EntryPoint> &entries) -> bool {
  std::string text;
  text += "// Generated by KukiShaderCompiler (engine/tools/compile_shaders.cpp) from engine/src/shader/*.hlsl.\n";
  text += "// Do not edit: edit the shader and rebuild.\n";
  text += "#include <compiled_shaders.hpp>\n#include <string_view>\nnamespace kuki {\nnamespace {\n";
  for (const auto &entry : entries) {
    text += std::format("alignas(4) constexpr unsigned char {}_{}[] = {{", entry.stem, entry.name);
    for (size_t i = 0; i < entry.bytecode.size(); ++i)
      text += std::format("{}{}", i % 32 == 0 ? "\n" : "", static_cast<unsigned>(entry.bytecode[i])) + ",";
    text += "\n};\n";
  }
  text += "struct Entry {\n  std::string_view file;\n  std::string_view name;\n  std::span<const unsigned char> bytecode;\n};\n";
  text += "constexpr Entry ENTRIES[]{\n";
  for (const auto &entry : entries)
    text += std::format("  {{\"{}\", \"{}\", {}_{}}},\n", entry.stem, entry.name, entry.stem, entry.name);
  text += "};\n} // namespace\n";
  // FNV-1a over every name and every byte, in the sorted order, so the same shaders give the same
  // digest on every machine. The pipeline library on disk is keyed on it: a pipeline the driver
  // compiled from other bytecode is not one to load.
  auto digest = 0xcbf29ce484222325ull;
  const auto Mix = [&](const auto &bytes) {
    for (const auto byte : bytes) {
      digest ^= static_cast<unsigned char>(byte);
      digest *= 0x100000001b3ull;
    }
  };
  for (const auto &entry : entries) {
    Mix(entry.stem);
    Mix(entry.name);
    Mix(entry.bytecode);
  }
  text += std::format("auto CompiledShadersDigest() -> uint64_t {{\n  return 0x{:016x}ull;\n}}\n", digest);
  text += "auto FindCompiledShader(const std::string_view file, const std::string_view entryPoint) -> std::span<const unsigned char> {\n";
  text += "  for (const auto &entry : ENTRIES)\n    if (entry.file == file && entry.name == entryPoint)\n      return entry.bytecode;\n  return {};\n}\n";
  text += "} // namespace kuki\n";
  // Written only when it changed, so a build that compiled the same bytes recompiles nothing after.
  std::string previous;
  if (ReadFile(options.output, previous) && previous == text)
    return true;
  std::ofstream stream(options.output, std::ios::binary);
  stream << text;
  return static_cast<bool>(stream);
}
} // namespace
auto main(const int argc, char **argv) -> int {
  Options options;
  if (!ParseOptions(argc, argv, options)) {
    std::fprintf(stderr, "usage: KukiShaderCompiler --output <file.cpp> [--debug --pdb <dir>] <shader.hlsl>...\n");
    return 2;
  }
  // By bare name, from beside the tool, where the build copies it with `dxil.dll` -- which it loads
  // the same way, and which a full path to `dxcompiler.dll` in the SDK would not find.
  auto *library = LoadLibraryW(L"dxcompiler.dll");
  const auto create = library ? reinterpret_cast<DxcCreateInstanceProc>(reinterpret_cast<void *>(GetProcAddress(library, "DxcCreateInstance"))) : nullptr;
  if (!create) {
    std::fprintf(stderr, "KukiShaderCompiler: dxcompiler.dll could not be loaded from beside the tool\n");
    return 1;
  }
  std::vector<std::string> sources(options.shaders.size());
  std::vector<EntryPoint> entries;
  for (size_t i = 0; i < options.shaders.size(); ++i) {
    if (!ReadFile(options.shaders[i], sources[i])) {
      std::fprintf(stderr, "%s: error: could not be read\n", options.shaders[i].string().c_str());
      return 1;
    }
    FindEntryPoints(options.shaders[i], sources[i], entries);
  }
  // Sorted, so the table and the file come out the same whatever order the build listed them in.
  std::ranges::sort(entries, [](const EntryPoint &a, const EntryPoint &b) { return std::tie(a.stem, a.name) < std::tie(b.stem, b.name); });
  if (options.debug)
    std::filesystem::create_directories(options.pdbDirectory);
  // In parallel, a compiler each: one `IDxcCompiler3` is not to be used from two threads at once.
  // Serially it was 15 s a configuration, which every edit to any shader would wait for.
  std::atomic<size_t> next{0};
  std::atomic<bool> failed{false};
  std::mutex output;
  const auto workers = std::max(1u, std::min(std::thread::hardware_concurrency(), static_cast<unsigned>(entries.size())));
  std::vector<std::jthread> threads;
  for (unsigned worker = 0; worker < workers; ++worker)
    threads.emplace_back([&] {
      Compiler compiler(create);
      if (!compiler.IsReady()) {
        failed = true;
        const std::scoped_lock lock(output);
        std::fprintf(stderr, "KukiShaderCompiler: dxcompiler.dll is too old to provide IDxcCompiler3\n");
        return;
      }
      for (auto index = next++; index < entries.size(); index = next++) {
        auto &entry = entries[index];
        const auto source = std::ranges::find(options.shaders, entry.file) - options.shaders.begin();
        if (const auto error = compiler.Compile(entry, sources[static_cast<size_t>(source)], options); !error.empty()) {
          failed = true;
          const std::scoped_lock lock(output);
          std::fprintf(stderr, "%s: error: %s (%s) did not compile:\n%s\n", entry.file.string().c_str(), entry.name.c_str(), entry.target.c_str(), error.c_str());
        }
      }
    });
  threads.clear();
  if (failed)
    return 1;
  if (!WriteSource(options, entries)) {
    std::fprintf(stderr, "KukiShaderCompiler: %s could not be written\n", options.output.string().c_str());
    return 1;
  }
  std::printf("KukiShaderCompiler: %zu entry points from %zu files\n", entries.size(), options.shaders.size());
  return 0;
}
