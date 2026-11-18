#include <dx_pipeline.hpp>
#ifdef KUKI_HAS_DIRECTX
#include <algorithm>
#include <compiled_shaders.hpp>
#include <format>
#include <fstream>
#include <functional>
#include <primitive.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <system_error>
namespace kuki {
struct DXPipelineJob {
  uint64_t key{};
  std::string label;
  bool compute{};
  D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsDesc{};
  D3D12_COMPUTE_PIPELINE_STATE_DESC computeDesc{};
  /// @brief The description's input layout, which it points into: the caller's was on its stack.
  std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
  ComPtr<ID3D12RootSignature> rootSignature;
  ComPtr<ID3D12Device> device;
  ComPtr<ID3D12PipelineState> result;
  bool loaded{};
  double milliseconds{};
  /// @brief Set by the worker last, after everything above it is written; read by the main thread
  /// before it touches any of it.
  std::atomic<bool> done{};
};
namespace {
auto MakeKey(const DXGI_FORMAT format, const uint32_t samples) -> uint64_t {
  return (static_cast<uint64_t>(format) << 32) | samples;
}
// A pipeline's name in the library is its key, which already says which shaders, which variant and
// which target it was made for.
auto LibraryName(const uint64_t key) -> std::wstring {
  return std::format(L"{:016x}", key);
}
// What the file starts with, before the driver's own serialised library. The driver checks its part
// against itself; this checks that the library was made from these shaders, which the driver cannot.
constexpr char LIBRARY_MAGIC[4]{'K', 'P', 'S', 'O'};
constexpr uint32_t LIBRARY_VERSION = 1;
struct LibraryHeader {
  char magic[4]{};
  uint32_t version{};
  uint64_t shaders{};
  uint64_t bytes{};
};
auto ReadLibraryFile(const std::filesystem::path &path, std::vector<char> &blob) -> bool {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return false;
  LibraryHeader header{};
  in.read(reinterpret_cast<char *>(&header), sizeof(header));
  if (!in || !std::equal(std::begin(header.magic), std::end(header.magic), std::begin(LIBRARY_MAGIC)) || header.version != LIBRARY_VERSION || header.bytes == 0)
    return false;
  if (header.shaders != CompiledShadersDigest()) {
    spdlog::info("[DX12] The pipeline library at {} was made from other shaders, and is made again", path.string());
    return false;
  }
  blob.resize(static_cast<size_t>(header.bytes));
  in.read(blob.data(), static_cast<std::streamsize>(blob.size()));
  return in.gcount() == static_cast<std::streamsize>(blob.size());
}
auto WriteLibraryFile(const std::filesystem::path &path, const std::vector<char> &blob) -> bool {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error)
    return false;
  // A temporary of this process's own and a rename, so a game closed while another copy of it is
  // closing leaves one whole library rather than two halves of one.
  auto temporary = path;
  temporary += std::format(".{}.tmp", GetCurrentProcessId());
  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out)
      return false;
    LibraryHeader header{};
    std::copy(std::begin(LIBRARY_MAGIC), std::end(LIBRARY_MAGIC), header.magic);
    header.version = LIBRARY_VERSION;
    header.shaders = CompiledShadersDigest();
    header.bytes = blob.size();
    out.write(reinterpret_cast<const char *>(&header), sizeof(header));
    out.write(blob.data(), static_cast<std::streamsize>(blob.size()));
    if (!out)
      return false;
  }
  std::filesystem::rename(temporary, path, error);
  if (error) {
    std::filesystem::remove(temporary, error);
    return false;
  }
  return true;
}
// Compiled when the engine was built (`FindCompiledShader`). Empty only for a name no shader
// defines, which is a mistake in the C++ rather than in a shader: one that did not compile would
// have failed the build.
auto FindShader(const char *file, const char *entryPoint) -> D3D12_SHADER_BYTECODE {
  const auto bytecode = FindCompiledShader(file, entryPoint);
  if (bytecode.empty())
    spdlog::error("[DX12] {}.hlsl has no entry point {}", file, entryPoint);
  return CD3DX12_SHADER_BYTECODE(bytecode.data(), bytecode.size());
}
} // namespace
auto DXPipelineCache::GetSceneRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (sceneRootSignature)
    return sceneRootSignature.Get();
  CD3DX12_DESCRIPTOR_RANGE materialRange{};
  materialRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MATERIAL_TEXTURE_SLOTS, 0);
  CD3DX12_DESCRIPTOR_RANGE shadowRange{};
  shadowRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, MATERIAL_TEXTURE_SLOTS);
  CD3DX12_DESCRIPTOR_RANGE spotShadowRange{};
  spotShadowRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, MATERIAL_TEXTURE_SLOTS + 1);
  CD3DX12_DESCRIPTOR_RANGE environmentRange{};
  environmentRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, MATERIAL_TEXTURE_SLOTS + 4);
  CD3DX12_DESCRIPTOR_RANGE sceneColorRange{};
  sceneColorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, MATERIAL_TEXTURE_SLOTS + 10);
  CD3DX12_DESCRIPTOR_RANGE occlusionRange{};
  occlusionRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, MATERIAL_TEXTURE_SLOTS + 12);
  CD3DX12_ROOT_PARAMETER parameters[14]{};
  // Visible to the vertex stage too, which reads the material's sway and waves from them.
  parameters[0].InitAsConstants(sizeof(DXSceneConstants) / sizeof(uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_ALL);
  parameters[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL);
  parameters[2].InitAsDescriptorTable(1, &materialRange, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[3].InitAsDescriptorTable(1, &shadowRange, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[4].InitAsDescriptorTable(1, &spotShadowRange, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[5].InitAsShaderResourceView(MATERIAL_TEXTURE_SLOTS + 2, 0, D3D12_SHADER_VISIBILITY_VERTEX);
  parameters[6].InitAsShaderResourceView(MATERIAL_TEXTURE_SLOTS + 3, 0, D3D12_SHADER_VISIBILITY_VERTEX);
  parameters[7].InitAsDescriptorTable(1, &environmentRange, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[8].InitAsShaderResourceView(MATERIAL_TEXTURE_SLOTS + 7, 0, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[9].InitAsShaderResourceView(MATERIAL_TEXTURE_SLOTS + 8, 0, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[10].InitAsShaderResourceView(MATERIAL_TEXTURE_SLOTS + 9, 0, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[11].InitAsDescriptorTable(1, &sceneColorRange, D3D12_SHADER_VISIBILITY_PIXEL);
  // The entities of the pool being drawn, beside its matrices in parameter 5.
  parameters[12].InitAsShaderResourceView(MATERIAL_TEXTURE_SLOTS + 11, 0, D3D12_SHADER_VISIBILITY_VERTEX);
  // The shade in the creases, from the occlusion pass, read pixel for pixel.
  parameters[13].InitAsDescriptorTable(1, &occlusionRange, D3D12_SHADER_VISIBILITY_PIXEL);
  CD3DX12_STATIC_SAMPLER_DESC samplers[3]{};
  samplers[0] = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
  samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  samplers[1] = CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
  samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  samplers[2] = CD3DX12_STATIC_SAMPLER_DESC(2, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
  samplers[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(_countof(parameters), parameters, 3, samplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the scene root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&sceneRootSignature)), "CreateRootSignature"))
    return nullptr;
  return sceneRootSignature.Get();
}
auto DXPipelineCache::GetScenePipeline(ID3D12Device *device, const DXGI_FORMAT format, const uint32_t samples, const bool skinned, const bool blended) -> const DXPipeline * {
  if (!device)
    return nullptr;
  const auto variant = std::string(skinned ? "sceneSkinned" : "scene") + (blended ? "Blended" : "");
  const auto key = MakeKey(format, samples) ^ std::hash<std::string>{}(variant);
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetSceneRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = sceneRootSignature;
  const auto vertexShader = FindShader("scene", skinned ? "VSSkinned" : "VSMain");
  const auto pixelShader = FindShader("scene", "PSMain");
  if (!vertexShader.BytecodeLength || !pixelShader.BytecodeLength)
    return nullptr;
  const D3D12_INPUT_ELEMENT_DESC inputLayout[]{
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, texture), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, tangent), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    // Listed before the bones though it comes after them in `Vertex`: the unskinned pipeline is
    // this list less its last two.
    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0, offsetof(Vertex, boneIds), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, boneWeights), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.InputLayout = {inputLayout, static_cast<UINT>(skinned ? _countof(inputLayout) : _countof(inputLayout) - 2)};
  desc.pRootSignature = rootSignature;
  desc.VS = vertexShader;
  desc.PS = pixelShader;
  desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  desc.RasterizerState.FrontCounterClockwise = TRUE;
  desc.RasterizerState.MultisampleEnable = samples > 1 ? TRUE : FALSE;
  desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  if (blended) {
    auto &target = desc.BlendState.RenderTarget[0];
    target.BlendEnable = TRUE;
    target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    target.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    target.BlendOp = D3D12_BLEND_OP_ADD;
    target.SrcBlendAlpha = D3D12_BLEND_ONE;
    target.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  }
  desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  desc.SampleMask = UINT_MAX;
  desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  desc.NumRenderTargets = 2;
  desc.RTVFormats[0] = format;
  desc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = samples > 0 ? samples : 1;
  return Submit(device, key, pipeline, desc, std::format("{} {} scene pipeline (format {}, {}x MSAA)", skinned ? "skinned" : "static", blended ? "blended" : "opaque", static_cast<int>(format), samples));
}
auto DXPipelineCache::GetPostRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (postRootSignature)
    return postRootSignature.Get();
  CD3DX12_DESCRIPTOR_RANGE sourceRange{};
  sourceRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  CD3DX12_DESCRIPTOR_RANGE secondRange{};
  secondRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
  CD3DX12_ROOT_PARAMETER parameters[3]{};
  parameters[0].InitAsConstants(sizeof(DXPostConstants) / sizeof(uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[1].InitAsDescriptorTable(1, &sourceRange, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[2].InitAsDescriptorTable(1, &secondRange, D3D12_SHADER_VISIBILITY_PIXEL);
  CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(3, parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the post root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&postRootSignature)), "CreateRootSignature"))
    return nullptr;
  return postRootSignature.Get();
}
auto DXPipelineCache::GetPostPipeline(ID3D12Device *device, const DXGI_FORMAT format, const char *effect) -> const DXPipeline * {
  if (!device || !effect)
    return nullptr;
  const auto key = MakeKey(format, 0) ^ std::hash<std::string>{}(effect);
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetPostRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = postRootSignature;
  const auto vertexShader = FindShader("post", "VSMain");
  const auto pixelShader = FindShader("post", effect);
  if (!vertexShader.BytecodeLength || !pixelShader.BytecodeLength)
    return nullptr;
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.VS = vertexShader;
  desc.PS = pixelShader;
  desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  desc.DepthStencilState.DepthEnable = FALSE;
  desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
  desc.SampleMask = UINT_MAX;
  desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  desc.NumRenderTargets = 1;
  desc.RTVFormats[0] = format;
  desc.SampleDesc.Count = 1;
  return Submit(device, key, pipeline, desc, std::format("post pipeline: {}", effect));
}
auto DXPipelineCache::GetStylizeRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (stylizeRootSignature)
    return stylizeRootSignature.Get();
  CD3DX12_DESCRIPTOR_RANGE sourceRange{};
  sourceRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  CD3DX12_DESCRIPTOR_RANGE depthRange{};
  depthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
  CD3DX12_ROOT_PARAMETER parameters[3]{};
  parameters[0].InitAsConstants(sizeof(DXStylizeConstants) / sizeof(uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[1].InitAsDescriptorTable(1, &sourceRange, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[2].InitAsDescriptorTable(1, &depthRange, D3D12_SHADER_VISIBILITY_PIXEL);
  // Clamped, so the smear and the pulled-apart channels read the edge of the picture rather than
  // wrapping round to the other side of it.
  CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(_countof(parameters), parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the stylize root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&stylizeRootSignature)), "CreateRootSignature"))
    return nullptr;
  return stylizeRootSignature.Get();
}
auto DXPipelineCache::GetStylizePipeline(ID3D12Device *device, const DXGI_FORMAT format, const char *entryPoint) -> const DXPipeline * {
  if (!device || !entryPoint)
    return nullptr;
  const auto key = MakeKey(format, 0) ^ std::hash<std::string>{}(std::string("stylize") + entryPoint);
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetStylizeRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = stylizeRootSignature;
  const auto vertexShader = FindShader("stylize", "VSMain");
  const auto pixelShader = FindShader("stylize", entryPoint);
  if (!vertexShader.BytecodeLength || !pixelShader.BytecodeLength)
    return nullptr;
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.VS = vertexShader;
  desc.PS = pixelShader;
  desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  desc.DepthStencilState.DepthEnable = FALSE;
  desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
  desc.SampleMask = UINT_MAX;
  desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  desc.NumRenderTargets = 1;
  desc.RTVFormats[0] = format;
  desc.SampleDesc.Count = 1;
  return Submit(device, key, pipeline, desc, std::format("stylize pipeline: {}", entryPoint));
}
auto DXPipelineCache::GetReflectionRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (reflectionRootSignature)
    return reflectionRootSignature.Get();
  CD3DX12_DESCRIPTOR_RANGE sourceRange{};
  sourceRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  CD3DX12_DESCRIPTOR_RANGE depthRange{};
  depthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
  CD3DX12_ROOT_PARAMETER parameters[3]{};
  parameters[0].InitAsConstants(sizeof(DXReflectionConstants) / sizeof(uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[1].InitAsDescriptorTable(1, &sourceRange, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[2].InitAsDescriptorTable(1, &depthRange, D3D12_SHADER_VISIBILITY_PIXEL);
  CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(_countof(parameters), parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the reflection root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&reflectionRootSignature)), "CreateRootSignature"))
    return nullptr;
  return reflectionRootSignature.Get();
}
auto DXPipelineCache::GetReflectionPipeline(ID3D12Device *device, const DXGI_FORMAT format) -> const DXPipeline * {
  if (!device)
    return nullptr;
  const auto key = MakeKey(format, 0) ^ std::hash<std::string>{}("reflection");
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetReflectionRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = reflectionRootSignature;
  const auto vertexShader = FindShader("reflect", "VSMain");
  const auto pixelShader = FindShader("reflect", "PSReflect");
  if (!vertexShader.BytecodeLength || !pixelShader.BytecodeLength)
    return nullptr;
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.VS = vertexShader;
  desc.PS = pixelShader;
  desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  desc.DepthStencilState.DepthEnable = FALSE;
  desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
  desc.SampleMask = UINT_MAX;
  desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  desc.NumRenderTargets = 1;
  desc.RTVFormats[0] = format;
  desc.SampleDesc.Count = 1;
  return Submit(device, key, pipeline, desc, "reflection pipeline");
}
auto DXPipelineCache::GetFullscreenRootSignature(ID3D12Device *device, ComPtr<ID3D12RootSignature> &signature, const uint32_t constantDwords, const char *name) -> ID3D12RootSignature * {
  if (signature)
    return signature.Get();
  CD3DX12_DESCRIPTOR_RANGE firstRange{};
  firstRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  CD3DX12_DESCRIPTOR_RANGE secondRange{};
  secondRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
  CD3DX12_ROOT_PARAMETER parameters[3]{};
  parameters[0].InitAsConstants(constantDwords, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[1].InitAsDescriptorTable(1, &firstRange, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[2].InitAsDescriptorTable(1, &secondRange, D3D12_SHADER_VISIBILITY_PIXEL);
  CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(_countof(parameters), parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the {} root signature: {}", name, log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&signature)), "CreateRootSignature"))
    return nullptr;
  return signature.Get();
}
auto DXPipelineCache::GetFullscreenPipeline(ID3D12Device *device, const DXGI_FORMAT format, const ComPtr<ID3D12RootSignature> &rootSignature, const char *file, const char *name, const char *entryPoint) -> const DXPipeline * {
  if (!device || !rootSignature || !entryPoint)
    return nullptr;
  const auto key = MakeKey(format, 0) ^ std::hash<std::string>{}(std::string(name) + entryPoint);
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  pipeline.rootSignature = rootSignature;
  const auto vertexShader = FindShader(file, "VSMain");
  const auto pixelShader = FindShader(file, entryPoint);
  if (!vertexShader.BytecodeLength || !pixelShader.BytecodeLength)
    return nullptr;
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature.Get();
  desc.VS = vertexShader;
  desc.PS = pixelShader;
  desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  desc.DepthStencilState.DepthEnable = FALSE;
  desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
  desc.SampleMask = UINT_MAX;
  desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  desc.NumRenderTargets = 1;
  desc.RTVFormats[0] = format;
  desc.SampleDesc.Count = 1;
  return Submit(device, key, pipeline, desc, std::format("{} pipeline: {}", name, entryPoint));
}
auto DXPipelineCache::GetOcclusionPipeline(ID3D12Device *device, const DXGI_FORMAT format, const char *entryPoint) -> const DXPipeline * {
  if (!device || !GetFullscreenRootSignature(device, occlusionRootSignature, sizeof(DXOcclusionConstants) / sizeof(uint32_t), "occlusion"))
    return nullptr;
  return GetFullscreenPipeline(device, format, occlusionRootSignature, "occlusion", "occlusion", entryPoint);
}
auto DXPipelineCache::GetFocusPipeline(ID3D12Device *device, const DXGI_FORMAT format) -> const DXPipeline * {
  if (!device || !GetFullscreenRootSignature(device, focusRootSignature, sizeof(DXFocusConstants) / sizeof(uint32_t), "focus"))
    return nullptr;
  return GetFullscreenPipeline(device, format, focusRootSignature, "depth_of_field", "focus", "PSFocus");
}
auto DXPipelineCache::GetOverlayRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (overlayRootSignature)
    return overlayRootSignature.Get();
  CD3DX12_DESCRIPTOR_RANGE atlasRange{};
  atlasRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  CD3DX12_ROOT_PARAMETER parameters[2]{};
  // Visible to both stages: the vertex shader reads the projection out of the same block the pixel
  // shader reads the colour from, and splitting one struct across two root entries to narrow the
  // visibility would cost a binding to save nothing.
  parameters[0].InitAsConstants(sizeof(DXOverlayConstants) / sizeof(uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_ALL);
  parameters[1].InitAsDescriptorTable(1, &atlasRange, D3D12_SHADER_VISIBILITY_PIXEL);
  // Clamped, because a glyph sits inside the atlas with a margin around it and wrapping could only
  // ever fetch a neighbour's ink.
  CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(2, parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the overlay root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&overlayRootSignature)), "CreateRootSignature"))
    return nullptr;
  return overlayRootSignature.Get();
}
auto DXPipelineCache::GetOverlayPipeline(ID3D12Device *device, const DXGI_FORMAT format) -> const DXPipeline * {
  if (!device)
    return nullptr;
  const auto key = MakeKey(format, 0) ^ std::hash<std::string>{}("overlay");
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetOverlayRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = overlayRootSignature;
  const auto vertexShader = FindShader("overlay", "VSMain");
  const auto pixelShader = FindShader("overlay", "PSMain");
  if (!vertexShader.BytecodeLength || !pixelShader.BytecodeLength)
    return nullptr;
  // The buffer is laid out as `Vertex` like any other mesh, so the stride steps over a normal and
  // a tangent that nothing here names. Two elements is what the text reads.
  const D3D12_INPUT_ELEMENT_DESC inputLayout[]{
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, texture), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.InputLayout = {inputLayout, _countof(inputLayout)};
  desc.pRootSignature = rootSignature;
  desc.VS = vertexShader;
  desc.PS = pixelShader;
  desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  auto &target = desc.BlendState.RenderTarget[0];
  target.BlendEnable = TRUE;
  target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
  target.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  target.BlendOp = D3D12_BLEND_OP_ADD;
  target.SrcBlendAlpha = D3D12_BLEND_ONE;
  target.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
  target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
  desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  desc.DepthStencilState.DepthEnable = FALSE;
  desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
  desc.SampleMask = UINT_MAX;
  desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  desc.NumRenderTargets = 1;
  desc.RTVFormats[0] = format;
  desc.SampleDesc.Count = 1;
  return Submit(device, key, pipeline, desc, "overlay pipeline");
}
auto DXPipelineCache::GetShadowRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (shadowRootSignature)
    return shadowRootSignature.Get();
  // The bones are a root view of their own, read only by the skinned variant. One signature for
  // both, so a pass drawing the two kinds switches the pipeline and nothing else.
  CD3DX12_ROOT_PARAMETER parameters[3]{};
  parameters[0].InitAsConstants(sizeof(DXShadowConstants) / sizeof(uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
  parameters[1].InitAsShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
  parameters[2].InitAsShaderResourceView(1, 0, D3D12_SHADER_VISIBILITY_VERTEX);
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(_countof(parameters), parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the shadow root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&shadowRootSignature)), "CreateRootSignature"))
    return nullptr;
  return shadowRootSignature.Get();
}
auto DXPipelineCache::GetShadowPipeline(ID3D12Device *device, const bool skinned) -> const DXPipeline * {
  if (!device)
    return nullptr;
  const auto key = std::hash<std::string>{}(skinned ? "shadowSkinned" : "shadow");
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetShadowRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = shadowRootSignature;
  const auto vertexShader = FindShader("shadow", skinned ? "VSSkinned" : "VSMain");
  if (!vertexShader.BytecodeLength)
    return nullptr;
  const D3D12_INPUT_ELEMENT_DESC inputLayout[]{
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, texture), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, tangent), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0, offsetof(Vertex, boneIds), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, boneWeights), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.InputLayout = {inputLayout, static_cast<UINT>(skinned ? _countof(inputLayout) : _countof(inputLayout) - 2)};
  desc.pRootSignature = rootSignature;
  desc.VS = vertexShader;
  desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  desc.RasterizerState.FrontCounterClockwise = TRUE;
  desc.RasterizerState.DepthBias = 1000;
  desc.RasterizerState.SlopeScaledDepthBias = 1.5f;
  desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  desc.SampleMask = UINT_MAX;
  desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  desc.NumRenderTargets = 0;
  desc.SampleDesc.Count = 1;
  return Submit(device, key, pipeline, desc, std::format("{} shadow pipeline", skinned ? "skinned" : "static"));
}
auto DXPipelineCache::GetSkyboxRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (skyboxRootSignature)
    return skyboxRootSignature.Get();
  // Two: the sky cubemap, and the one texel holding what the sun's own colour has become on its
  // way down to the viewer.
  CD3DX12_DESCRIPTOR_RANGE equirectRange{};
  equirectRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, SKYBOX_SRV_SLOTS, 0);
  CD3DX12_ROOT_PARAMETER parameters[2]{};
  parameters[0].InitAsConstants(sizeof(DXSkyboxConstants) / sizeof(uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[1].InitAsDescriptorTable(1, &equirectRange, D3D12_SHADER_VISIBILITY_PIXEL);
  CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(2, parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the skybox root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&skyboxRootSignature)), "CreateRootSignature"))
    return nullptr;
  return skyboxRootSignature.Get();
}
auto DXPipelineCache::GetSkyboxPipeline(ID3D12Device *device, const DXGI_FORMAT format, const uint32_t samples) -> const DXPipeline * {
  if (!device)
    return nullptr;
  const auto key = MakeKey(format, samples) ^ std::hash<std::string>{}("skybox");
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetSkyboxRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = skyboxRootSignature;
  const auto vertexShader = FindShader("skybox", "VSMain");
  const auto pixelShader = FindShader("skybox", "PSMain");
  if (!vertexShader.BytecodeLength || !pixelShader.BytecodeLength)
    return nullptr;
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.VS = vertexShader;
  desc.PS = pixelShader;
  desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  desc.RasterizerState.MultisampleEnable = samples > 1 ? TRUE : FALSE;
  desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  desc.DepthStencilState.DepthEnable = FALSE;
  desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  desc.SampleMask = UINT_MAX;
  desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  desc.NumRenderTargets = 2;
  desc.RTVFormats[0] = format;
  desc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = samples > 0 ? samples : 1;
  return Submit(device, key, pipeline, desc, std::format("skybox pipeline (format {}, {}x MSAA)", static_cast<int>(format), samples));
}
auto DXPipelineCache::GetComputeRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (computeRootSignature)
    return computeRootSignature.Get();
  CD3DX12_DESCRIPTOR_RANGE srvRange{};
  srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, COMPUTE_SRV_SLOTS, 0);
  CD3DX12_DESCRIPTOR_RANGE uavRange{};
  uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, COMPUTE_UAV_SLOTS, 0);
  CD3DX12_ROOT_PARAMETER parameters[3]{};
  parameters[0].InitAsConstants(sizeof(DXIBLConstants) / sizeof(uint32_t), 0);
  parameters[1].InitAsDescriptorTable(1, &srvRange);
  parameters[2].InitAsDescriptorTable(1, &uavRange);
  CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(3, parameters, 1, &sampler);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the compute root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature)), "CreateRootSignature"))
    return nullptr;
  return computeRootSignature.Get();
}
auto DXPipelineCache::GetAtmosphereRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (atmosphereRootSignature)
    return atmosphereRootSignature.Get();
  CD3DX12_DESCRIPTOR_RANGE srvRange{};
  srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, ATMOSPHERE_SRV_SLOTS, 0);
  CD3DX12_DESCRIPTOR_RANGE uavRange{};
  uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, ATMOSPHERE_UAV_SLOTS, 0);
  CD3DX12_ROOT_PARAMETER parameters[3]{};
  parameters[0].InitAsConstants(sizeof(DXAtmosphereConstants) / sizeof(uint32_t), 0);
  parameters[1].InitAsDescriptorTable(1, &srvRange);
  parameters[2].InitAsDescriptorTable(1, &uavRange);
  // Clamped on every axis, which the transmittance table depends on rather than merely tolerates:
  // its edges are the horizon and the top of the atmosphere, and a wrapped sample there would read
  // the opposite extreme of the sky.
  CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(3, parameters, 1, &sampler);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the atmosphere root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&atmosphereRootSignature)), "CreateRootSignature"))
    return nullptr;
  return atmosphereRootSignature.Get();
}
auto DXPipelineCache::GetAtmospherePipeline(ID3D12Device *device, const char *entryPoint) -> const DXPipeline * {
  if (!device || !entryPoint)
    return nullptr;
  const auto key = std::hash<std::string>{}(std::string("atmosphere:") + entryPoint);
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetAtmosphereRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = atmosphereRootSignature;
  const auto computeShader = FindShader("atmosphere", entryPoint);
  if (!computeShader.BytecodeLength)
    return nullptr;
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.CS = computeShader;
  return Submit(device, key, pipeline, desc, std::format("atmosphere pipeline: {}", entryPoint));
}
auto DXPipelineCache::GetCloudRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (cloudRootSignature)
    return cloudRootSignature.Get();
  CD3DX12_DESCRIPTOR_RANGE srvRange{};
  srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, CLOUD_SRV_SLOTS, 0);
  CD3DX12_DESCRIPTOR_RANGE uavRange{};
  uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, CLOUD_UAV_SLOTS, 0);
  CD3DX12_ROOT_PARAMETER parameters[3]{};
  parameters[0].InitAsConstants(sizeof(DXCloudConstants) / sizeof(uint32_t), 0);
  parameters[1].InitAsDescriptorTable(1, &srvRange);
  parameters[2].InitAsDescriptorTable(1, &uavRange);
  // Three samplers because three things are being sampled and no two of them want the same
  // addressing. The noise volumes tile and are read with coordinates that grow without limit as the
  // layer drifts, so they must wrap; the transmittance table's edges are the horizon and the top of
  // the atmosphere, where a wrapped sample reads the opposite extreme of the sky; and the depth
  // buffer is compared rather than blended, so filtering it would invent surfaces between two that
  // are really there.
  CD3DX12_STATIC_SAMPLER_DESC samplers[3]{};
  samplers[0] = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
  samplers[1] = CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
  samplers[2] = CD3DX12_STATIC_SAMPLER_DESC(2, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(3, parameters, 3, samplers);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the cloud root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&cloudRootSignature)), "CreateRootSignature"))
    return nullptr;
  return cloudRootSignature.Get();
}
auto DXPipelineCache::GetVolumetricRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (volumetricRootSignature)
    return volumetricRootSignature.Get();
  CD3DX12_DESCRIPTOR_RANGE srvRange{};
  srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, VOLUMETRIC_SRV_SLOTS, 0);
  CD3DX12_ROOT_PARAMETER parameters[3]{};
  parameters[0].InitAsConstants(sizeof(DXVolumetricConstants) / sizeof(uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
  // The shading pass's own constant buffer, bound unchanged. See `DXVolumetricConstants` for why
  // the lights are read out of it rather than gathered a second time.
  parameters[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[2].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);
  CD3DX12_STATIC_SAMPLER_DESC samplers[2]{};
  samplers[0] = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
  samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  samplers[1] = CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
  samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(3, parameters, 2, samplers);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the volumetric root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&volumetricRootSignature)), "CreateRootSignature"))
    return nullptr;
  return volumetricRootSignature.Get();
}
auto DXPipelineCache::GetCloudComputePipeline(ID3D12Device *device, const char *entryPoint) -> const DXPipeline * {
  if (!device || !entryPoint)
    return nullptr;
  const auto key = std::hash<std::string>{}(std::string("cloud:") + entryPoint);
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetCloudRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = cloudRootSignature;
  const auto computeShader = FindShader("clouds", entryPoint);
  if (!computeShader.BytecodeLength)
    return nullptr;
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.CS = computeShader;
  return Submit(device, key, pipeline, desc, std::format("cloud compute pipeline: {}", entryPoint));
}
auto DXPipelineCache::GetCloudPipeline(ID3D12Device *device, const DXGI_FORMAT format) -> const DXPipeline * {
  if (!device)
    return nullptr;
  const auto key = MakeKey(format, 0) ^ std::hash<std::string>{}("cloudDraw");
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetCloudRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = cloudRootSignature;
  const auto vertexShader = FindShader("clouds", "VSMain");
  const auto pixelShader = FindShader("clouds", "PSMain");
  if (!vertexShader.BytecodeLength || !pixelShader.BytecodeLength)
    return nullptr;
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.VS = vertexShader;
  desc.PS = pixelShader;
  desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  desc.DepthStencilState.DepthEnable = FALSE;
  desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
  desc.SampleMask = UINT_MAX;
  desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  desc.NumRenderTargets = 1;
  desc.RTVFormats[0] = format;
  desc.SampleDesc.Count = 1;
  return Submit(device, key, pipeline, desc, "cloud pipeline");
}
auto DXPipelineCache::GetVolumetricPipeline(ID3D12Device *device, const DXGI_FORMAT format) -> const DXPipeline * {
  if (!device)
    return nullptr;
  const auto key = MakeKey(format, 0) ^ std::hash<std::string>{}("volumetric");
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetVolumetricRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = volumetricRootSignature;
  const auto vertexShader = FindShader("volumetric", "VSMain");
  const auto pixelShader = FindShader("volumetric", "PSMain");
  if (!vertexShader.BytecodeLength || !pixelShader.BytecodeLength)
    return nullptr;
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.VS = vertexShader;
  desc.PS = pixelShader;
  desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  desc.DepthStencilState.DepthEnable = FALSE;
  desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
  desc.SampleMask = UINT_MAX;
  desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  desc.NumRenderTargets = 1;
  desc.RTVFormats[0] = format;
  desc.SampleDesc.Count = 1;
  return Submit(device, key, pipeline, desc, "volumetric pipeline");
}
auto DXPipelineCache::GetParticleUpdateRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (particleUpdateRootSignature)
    return particleUpdateRootSignature.Get();
  CD3DX12_ROOT_PARAMETER parameters[3]{};
  parameters[0].InitAsConstants(sizeof(DXParticleUpdateConstants) / sizeof(uint32_t), 0);
  parameters[1].InitAsUnorderedAccessView(0);
  // The surface samples, at register one. A root descriptor like the pool, so that neither costs a
  // slot in the descriptor heap -- and so one always has to be bound, mesh or no mesh. See
  // `particleNoSamples` in the renderer.
  parameters[2].InitAsShaderResourceView(1);
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(3, parameters);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the particle update root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&particleUpdateRootSignature)), "CreateRootSignature"))
    return nullptr;
  return particleUpdateRootSignature.Get();
}
auto DXPipelineCache::GetParticleUpdatePipeline(ID3D12Device *device) -> const DXPipeline * {
  if (!device)
    return nullptr;
  const auto key = std::hash<std::string>{}("particles:CSSimulate");
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetParticleUpdateRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = particleUpdateRootSignature;
  const auto computeShader = FindShader("particles", "CSSimulate");
  if (!computeShader.BytecodeLength)
    return nullptr;
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.CS = computeShader;
  return Submit(device, key, pipeline, desc, "particle update pipeline");
}
auto DXPipelineCache::GetParticleDrawRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (particleDrawRootSignature)
    return particleDrawRootSignature.Get();
  CD3DX12_ROOT_PARAMETER parameters[2]{};
  // Register one, where `particles.hlsl` puts the draw's block to keep it clear of the step's.
  parameters[0].InitAsConstants(sizeof(DXParticleDrawConstants) / sizeof(uint32_t), 1, 0, D3D12_SHADER_VISIBILITY_VERTEX);
  parameters[1].InitAsShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(2, parameters);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the particle draw root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&particleDrawRootSignature)), "CreateRootSignature"))
    return nullptr;
  return particleDrawRootSignature.Get();
}
auto DXPipelineCache::GetParticleDrawPipeline(ID3D12Device *device, const DXGI_FORMAT format, const ParticleBlend mode) -> const DXPipeline * {
  if (!device)
    return nullptr;
  const auto opaque = mode == ParticleBlend::Opaque;
  const auto veil = mode == ParticleBlend::Alpha;
  const char *name = opaque ? "particleDrawOpaque" : veil ? "particleDrawAlpha" : "particleDraw";
  const auto key = MakeKey(format, 0) ^ std::hash<std::string>{}(name);
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetParticleDrawRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = particleDrawRootSignature;
  const auto vertexShader = FindShader("particles", "VSMain");
  const auto pixelShader = FindShader("particles", opaque ? "PSOpaque" : veil ? "PSAlpha" : "PSMain");
  if (!vertexShader.BytecodeLength || !pixelShader.BytecodeLength)
    return nullptr;
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.VS = vertexShader;
  desc.PS = pixelShader;
  desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  // A quad built from the camera's own axes has no back to cull, and winding it to suit a culling
  // mode would be an order to keep right for nothing.
  desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  // One plus one: what is there, and the particle on top of it. Addition does not care what order
  // its terms arrive in, which is the property the whole pass leans on in place of a sort.
  //
  // The alpha channel is left out of the write altogether rather than blended to no effect. It
  // belongs to the picture underneath, and nothing a particle could put there would mean anything.
  desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  auto &blend = desc.BlendState.RenderTarget[0];
  blend.BlendEnable = TRUE;
  blend.SrcBlend = D3D12_BLEND_ONE;
  blend.DestBlend = D3D12_BLEND_ONE;
  blend.BlendOp = D3D12_BLEND_OP_ADD;
  blend.SrcBlendAlpha = D3D12_BLEND_ZERO;
  blend.DestBlendAlpha = D3D12_BLEND_ONE;
  blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
  blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
  // The solid kind replaces what is there, and keeps the same write mask for the same reason.
  if (opaque)
    blend.BlendEnable = FALSE;
  // The veil: the particle's premultiplied colour, over what is behind scaled by what it lets
  // through. Order matters for this one in principle, and is not kept -- see `ParticleBlend::Alpha`.
  if (veil)
    blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  // Tested and not written, for the additive kind. A spark is hidden by the scene in front of it
  // and hides nothing itself -- writing depth would make which of two overlapping sparks survives
  // depend on which instance came first, and bring the ordering problem back through the depth
  // buffer. For the solid kind that is not a problem but the answer: the nearer disc should win,
  // whichever came first, and writing depth is what makes it.
  desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  desc.DepthStencilState.DepthWriteMask = opaque ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
  desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  desc.SampleMask = UINT_MAX;
  desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  desc.NumRenderTargets = 1;
  desc.RTVFormats[0] = format;
  desc.SampleDesc.Count = 1;
  return Submit(device, key, pipeline, desc, std::format("{} particle draw pipeline", opaque ? "opaque" : veil ? "alpha"
                                                                                                               : "additive"));
}
auto DXPipelineCache::GetComputePipeline(ID3D12Device *device, const char *entryPoint) -> const DXPipeline * {
  if (!device || !entryPoint)
    return nullptr;
  const auto key = std::hash<std::string>{}(std::string("compute:") + entryPoint);
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetComputeRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = computeRootSignature;
  const auto computeShader = FindShader("ibl", entryPoint);
  if (!computeShader.BytecodeLength)
    return nullptr;
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.CS = computeShader;
  return Submit(device, key, pipeline, desc, std::format("compute pipeline: {}", entryPoint));
}
auto DXPipelineCache::GetPickRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (pickRootSignature)
    return pickRootSignature.Get();
  CD3DX12_DESCRIPTOR_RANGE srvRange{};
  srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  CD3DX12_ROOT_PARAMETER parameters[3]{};
  parameters[0].InitAsConstants(sizeof(DXPickConstants) / sizeof(uint32_t), 0);
  parameters[1].InitAsDescriptorTable(1, &srvRange);
  parameters[2].InitAsUnorderedAccessView(0);
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(3, parameters);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the pick root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&pickRootSignature)), "CreateRootSignature"))
    return nullptr;
  return pickRootSignature.Get();
}
auto DXPipelineCache::GetPickPipeline(ID3D12Device *device) -> const DXPipeline * {
  if (!device)
    return nullptr;
  const auto key = std::hash<std::string>{}("pick:CSPick");
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetPickRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = pickRootSignature;
  const auto computeShader = FindShader("pick", "CSPick");
  if (!computeShader.BytecodeLength)
    return nullptr;
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.CS = computeShader;
  return Submit(device, key, pipeline, desc, "pick pipeline");
}
auto DXPipelineCache::GetRayProbeRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (rayProbeRootSignature)
    return rayProbeRootSignature.Get();
  CD3DX12_ROOT_PARAMETER parameters[4]{};
  parameters[0].InitAsConstants(sizeof(DXRayProbeConstants) / sizeof(uint32_t), 0);
  parameters[1].InitAsShaderResourceView(0);
  parameters[2].InitAsShaderResourceView(1);
  parameters[3].InitAsUnorderedAccessView(0);
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(4, parameters);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the ray probe root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&rayProbeRootSignature)), "CreateRootSignature"))
    return nullptr;
  return rayProbeRootSignature.Get();
}
auto DXPipelineCache::GetRayProbePipeline(ID3D12Device *device) -> const DXPipeline * {
  if (!device)
    return nullptr;
  const auto key = std::hash<std::string>{}("raytrace:CSProbe");
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetRayProbeRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = rayProbeRootSignature;
  const auto computeShader = FindShader("raytrace", "CSProbe");
  if (!computeShader.BytecodeLength)
    return nullptr;
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.CS = computeShader;
  return Submit(device, key, pipeline, desc, "ray probe pipeline (shader model 6.5, inline raytracing)");
}
auto DXPipelineCache::GetProbeAuditRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (probeAuditRootSignature)
    return probeAuditRootSignature.Get();
  CD3DX12_ROOT_PARAMETER parameters[5]{};
  parameters[0].InitAsConstants(sizeof(DXProbeVolumeConstants) / sizeof(uint32_t), 0);
  parameters[1].InitAsShaderResourceView(0);
  parameters[2].InitAsShaderResourceView(1);
  parameters[3].InitAsShaderResourceView(2);
  parameters[4].InitAsUnorderedAccessView(0);
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(5, parameters);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the probe audit root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&probeAuditRootSignature)), "CreateRootSignature"))
    return nullptr;
  return probeAuditRootSignature.Get();
}
auto DXPipelineCache::GetProbeAuditPipeline(ID3D12Device *device) -> const DXPipeline * {
  if (!device)
    return nullptr;
  const auto key = std::hash<std::string>{}("probe:CSAudit");
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetProbeAuditRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = probeAuditRootSignature;
  const auto computeShader = FindShader("probe", "CSAudit");
  if (!computeShader.BytecodeLength)
    return nullptr;
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.CS = computeShader;
  return Submit(device, key, pipeline, desc, "probe audit pipeline");
}
auto DXPipelineCache::GetProbeTraceRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (probeTraceRootSignature)
    return probeTraceRootSignature.Get();
  CD3DX12_DESCRIPTOR_RANGE meshRange{};
  meshRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, 0, 1);
  CD3DX12_DESCRIPTOR_RANGE textureRange{};
  textureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, 0, 2);
  CD3DX12_ROOT_PARAMETER parameters[10]{};
  parameters[0].InitAsConstants(sizeof(DXProbeTraceConstants) / sizeof(uint32_t), 0);
  parameters[1].InitAsConstantBufferView(1);
  parameters[2].InitAsShaderResourceView(0);
  parameters[3].InitAsShaderResourceView(1);
  parameters[4].InitAsShaderResourceView(2);
  parameters[5].InitAsShaderResourceView(3);
  parameters[6].InitAsUnorderedAccessView(0);
  parameters[7].InitAsDescriptorTable(1, &meshRange);
  parameters[8].InitAsDescriptorTable(1, &textureRange);
  parameters[9].InitAsShaderResourceView(4);
  CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(10, parameters, 1, &sampler);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the probe trace root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&probeTraceRootSignature)), "CreateRootSignature"))
    return nullptr;
  return probeTraceRootSignature.Get();
}
auto DXPipelineCache::GetProbeTracePipeline(ID3D12Device *device, const char *entryPoint) -> const DXPipeline * {
  if (!device || !entryPoint)
    return nullptr;
  const auto key = std::hash<std::string>{}(std::string("probe_trace:") + entryPoint);
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetProbeTraceRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = probeTraceRootSignature;
  const auto computeShader = FindShader("probe_trace", entryPoint);
  if (!computeShader.BytecodeLength)
    return nullptr;
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.CS = computeShader;
  return Submit(device, key, pipeline, desc, std::format("probe trace pipeline {} (shader model 6.5, inline raytracing, bindless)", entryPoint));
}
auto DXPipelineCache::GetProbeUpdateRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (probeUpdateRootSignature)
    return probeUpdateRootSignature.Get();
  CD3DX12_ROOT_PARAMETER parameters[10]{};
  parameters[0].InitAsConstants(sizeof(DXProbeUpdateConstants) / sizeof(uint32_t), 0);
  parameters[1].InitAsShaderResourceView(0);
  parameters[2].InitAsShaderResourceView(1);
  for (uint32_t slot = 0; slot < 7; ++slot)
    parameters[3 + slot].InitAsUnorderedAccessView(slot);
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(10, parameters);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the probe update root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&probeUpdateRootSignature)), "CreateRootSignature"))
    return nullptr;
  return probeUpdateRootSignature.Get();
}
auto DXPipelineCache::GetProbeUpdatePipeline(ID3D12Device *device, const char *entryPoint) -> const DXPipeline * {
  if (!device || !entryPoint)
    return nullptr;
  const auto key = std::hash<std::string>{}(std::string("probe_update:") + entryPoint);
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetProbeUpdateRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = probeUpdateRootSignature;
  const auto computeShader = FindShader("probe_update", entryPoint);
  if (!computeShader.BytecodeLength)
    return nullptr;
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.CS = computeShader;
  return Submit(device, key, pipeline, desc, std::format("probe update pipeline {}", entryPoint));
}
auto DXPipelineCache::GetProbeDebugRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (probeDebugRootSignature)
    return probeDebugRootSignature.Get();
  CD3DX12_ROOT_PARAMETER parameters[2]{};
  parameters[0].InitAsConstants(sizeof(DXProbeDebugConstants) / sizeof(uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_ALL);
  parameters[1].InitAsShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(2, parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] Failed to serialise the probe debug root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&probeDebugRootSignature)), "CreateRootSignature"))
    return nullptr;
  return probeDebugRootSignature.Get();
}
auto DXPipelineCache::GetProbeDebugPipeline(ID3D12Device *device, const DXGI_FORMAT format, const uint32_t samples) -> const DXPipeline * {
  if (!device)
    return nullptr;
  const auto key = MakeKey(format, samples) ^ std::hash<std::string>{}("probeDebug");
  if (auto it = pipelines.find(key); it != pipelines.end())
    return Poll(key, it->second);
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetProbeDebugRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = probeDebugRootSignature;
  const auto vertexShader = FindShader("probe_debug", "VSMain");
  const auto pixelShader = FindShader("probe_debug", "PSMain");
  if (!vertexShader.BytecodeLength || !pixelShader.BytecodeLength)
    return nullptr;
  const D3D12_INPUT_ELEMENT_DESC inputLayout[]{
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.InputLayout = {inputLayout, _countof(inputLayout)};
  desc.pRootSignature = rootSignature;
  desc.VS = vertexShader;
  desc.PS = pixelShader;
  desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  desc.RasterizerState.FrontCounterClockwise = TRUE;
  desc.RasterizerState.MultisampleEnable = samples > 1 ? TRUE : FALSE;
  desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  desc.SampleMask = UINT_MAX;
  desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  desc.NumRenderTargets = 2;
  desc.RTVFormats[0] = format;
  desc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = samples > 0 ? samples : 1;
  return Submit(device, key, pipeline, desc, std::format("probe debug pipeline (format {}, {}x MSAA)", static_cast<int>(format), samples));
}
DXPipelineCache::~DXPipelineCache() {
  StopWorkers();
}
auto DXPipelineCache::SetLibraryPath(std::filesystem::path path) -> void {
  libraryPath = std::move(path);
}
auto DXPipelineCache::Poll(const uint64_t key, DXPipeline &pipeline) -> const DXPipeline * {
  if (pipeline)
    return &pipeline;
  const auto it = pending.find(key);
  if (it == pending.end() || !it->second->done.load(std::memory_order_acquire))
    return nullptr;
  auto &job = *it->second;
  pipeline.pipelineState = std::move(job.result);
  if (pipeline) {
    ++(job.loaded ? loadedCount : createdCount);
    spdlog::debug("[DX12] {} {} in {:.1f} ms", job.loaded ? "Loaded" : "Created", job.label, job.milliseconds);
  }
  pending.erase(it);
  // Once for each burst of requests rather than once for each pipeline: the first frames ask for
  // most of them at once, and how long the lot took to arrive is the number worth reading.
  if (pending.empty()) {
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - busySince).count();
    spdlog::info("[DX12] {} pipelines ready {:.0f} ms after the first was asked for: {} loaded from the library, {} compiled by the driver", loadedCount + createdCount, elapsed, loadedCount, createdCount);
    loadedCount = 0;
    createdCount = 0;
  }
  return pipeline ? &pipeline : nullptr;
}
auto DXPipelineCache::Collect() -> void {
  // `Poll` erases what it picks up, so the keys are gathered first. Only while something is
  // pending, which is the first frames and a setting changed: the rest of the time this is nothing.
  if (pending.empty())
    return;
  for (auto it = pending.begin(); it != pending.end();) {
    const auto key = (it++)->first;
    if (auto found = pipelines.find(key); found != pipelines.end())
      Poll(key, found->second);
  }
}
auto DXPipelineCache::Submit(ID3D12Device *device, const uint64_t key, DXPipeline &pipeline, const D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc, std::string label) -> const DXPipeline * {
  auto job = std::make_shared<DXPipelineJob>();
  job->key = key;
  job->label = std::move(label);
  job->graphicsDesc = desc;
  if (desc.InputLayout.NumElements > 0)
    job->inputLayout.assign(desc.InputLayout.pInputElementDescs, desc.InputLayout.pInputElementDescs + desc.InputLayout.NumElements);
  job->graphicsDesc.InputLayout = {job->inputLayout.empty() ? nullptr : job->inputLayout.data(), static_cast<UINT>(job->inputLayout.size())};
  job->rootSignature = pipeline.rootSignature;
  Enqueue(device, std::move(job));
  return nullptr;
}
auto DXPipelineCache::Submit(ID3D12Device *device, const uint64_t key, DXPipeline &pipeline, const D3D12_COMPUTE_PIPELINE_STATE_DESC &desc, std::string label) -> const DXPipeline * {
  auto job = std::make_shared<DXPipelineJob>();
  job->key = key;
  job->label = std::move(label);
  job->compute = true;
  job->computeDesc = desc;
  job->rootSignature = pipeline.rootSignature;
  Enqueue(device, std::move(job));
  return nullptr;
}
auto DXPipelineCache::Enqueue(ID3D12Device *device, std::shared_ptr<DXPipelineJob> job) -> void {
  // Started on the first request, not before: a run that draws nothing makes no threads. One core
  // is left to the main thread, which goes on presenting frames while these run; past four, the
  // driver's compile is the limit rather than the number of threads asking for it.
  if (workers.empty()) {
    const auto count = std::clamp(std::thread::hardware_concurrency(), 2u, 5u) - 1;
    for (unsigned worker = 0; worker < count; ++worker)
      workers.emplace_back([this](const std::stop_token stop) { Work(stop); });
  }
  job->device = device;
  if (pending.empty())
    busySince = std::chrono::steady_clock::now();
  pending[job->key] = job;
  {
    const std::scoped_lock lock(queueMutex);
    queue.push_back(std::move(job));
  }
  queueReady.notify_one();
}
auto DXPipelineCache::Work(const std::stop_token stop) -> void {
  for (;;) {
    std::shared_ptr<DXPipelineJob> job;
    {
      std::unique_lock lock(queueMutex);
      if (!queueReady.wait(lock, stop, [this] { return !queue.empty(); }))
        return;
      job = std::move(queue.front());
      queue.pop_front();
    }
    Run(*job);
  }
}
auto DXPipelineCache::Run(DXPipelineJob &job) -> void {
  const auto start = std::chrono::steady_clock::now();
  // Opened by whichever worker gets here first, and the others wait for it: reading the file and
  // having the driver parse it is a few milliseconds the main thread need not spend.
  {
    const std::scoped_lock lock(libraryMutex);
    if (!libraryOpened)
      OpenLibrary(job.device.Get());
  }
  // The library is safe to use from several threads at once for different names, and every key is
  // asked for once, so no two workers ever load the same name.
  const auto name = LibraryName(job.key);
  if (library) {
    const auto loaded = job.compute ? library->LoadComputePipeline(name.c_str(), &job.computeDesc, IID_PPV_ARGS(&job.result)) : library->LoadGraphicsPipeline(name.c_str(), &job.graphicsDesc, IID_PPV_ARGS(&job.result));
    job.loaded = SUCCEEDED(loaded);
  }
  if (!job.loaded) {
    job.result.Reset();
    const auto created = job.compute ? job.device->CreateComputePipelineState(&job.computeDesc, IID_PPV_ARGS(&job.result)) : job.device->CreateGraphicsPipelineState(&job.graphicsDesc, IID_PPV_ARGS(&job.result));
    if (DXFailed(created, std::format("Creating the {}", job.label)))
      job.result.Reset();
    else if (library) {
      // Refused with this code when the name is already taken, which after a failed load means the
      // library holds an older description under it.
      if (const auto stored = library->StorePipeline(name.c_str(), job.result.Get()); SUCCEEDED(stored))
        libraryChanged = true;
      else if (stored == E_INVALIDARG)
        libraryStale = true;
      else
        DXFailed(stored, "ID3D12PipelineLibrary::StorePipeline");
    }
  }
  job.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  job.done.store(true, std::memory_order_release);
}
auto DXPipelineCache::OpenLibrary(ID3D12Device *device) -> void {
  libraryOpened = true;
  if (libraryPath.empty() || !device || FAILED(device->QueryInterface(IID_PPV_ARGS(&libraryDevice))))
    return;
  D3D12_FEATURE_DATA_SHADER_CACHE support{};
  if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_CACHE, &support, sizeof(support))) || !(support.SupportFlags & D3D12_SHADER_CACHE_SUPPORT_LIBRARY)) {
    spdlog::info("[DX12] The driver keeps no pipeline library, so every run compiles its pipelines");
    libraryDevice.Reset();
    return;
  }
  if (ReadLibraryFile(libraryPath, libraryBlob))
    if (const auto opened = libraryDevice->CreatePipelineLibrary(libraryBlob.data(), libraryBlob.size(), IID_PPV_ARGS(&library)); FAILED(opened)) {
      spdlog::info("[DX12] The pipeline library at {} was refused ({}), and is made again", libraryPath.string(), DXResultToString(opened));
      library.Reset();
    }
  if (library)
    return;
  libraryBlob = {};
  if (DXFailed(libraryDevice->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&library)), "CreatePipelineLibrary")) {
    library.Reset();
    libraryDevice.Reset();
  }
}
auto DXPipelineCache::SaveLibrary() -> void {
  if (!library || !libraryDevice || libraryPath.empty() || (!libraryChanged && !libraryStale))
    return;
  // Made again from this run's pipelines when an old description held a name, since an entry
  // cannot be replaced in place. What this run did not ask for is lost from it, and made once more
  // the next time something asks.
  auto output = library;
  if (libraryStale) {
    output.Reset();
    if (DXFailed(libraryDevice->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&output)), "CreatePipelineLibrary"))
      return;
    for (const auto &[key, pipeline] : pipelines)
      if (pipeline.pipelineState)
        output->StorePipeline(LibraryName(key).c_str(), pipeline.pipelineState.Get());
  }
  std::vector<char> blob(output->GetSerializedSize());
  if (blob.empty() || DXFailed(output->Serialize(blob.data(), blob.size()), "ID3D12PipelineLibrary::Serialize"))
    return;
  if (WriteLibraryFile(libraryPath, blob))
    spdlog::info("[DX12] Saved the pipeline library ({:.1f} MB) to {}", static_cast<double>(blob.size()) / (1024. * 1024.), libraryPath.string());
  else
    spdlog::warn("[DX12] Could not save the pipeline library to {}", libraryPath.string());
}
auto DXPipelineCache::StopWorkers() -> void {
  // What has not been started is dropped; what has is waited for, since it holds the device.
  {
    const std::scoped_lock lock(queueMutex);
    queue.clear();
  }
  workers.clear();
}
auto DXPipelineCache::Clear() -> void {
  StopWorkers();
  SaveLibrary();
  pending.clear();
  loadedCount = 0;
  createdCount = 0;
  library.Reset();
  libraryBlob = {};
  libraryDevice.Reset();
  libraryOpened = false;
  libraryChanged = false;
  libraryStale = false;
  pipelines.clear();
  sceneRootSignature.Reset();
  postRootSignature.Reset();
  shadowRootSignature.Reset();
  skyboxRootSignature.Reset();
  computeRootSignature.Reset();
  // These four were missing, and a root signature that outlives a `Clear` is one the next pipeline
  // built against it silently shares with a device that may no longer be the same one.
  overlayRootSignature.Reset();
  atmosphereRootSignature.Reset();
  cloudRootSignature.Reset();
  volumetricRootSignature.Reset();
  particleUpdateRootSignature.Reset();
  particleDrawRootSignature.Reset();
  pickRootSignature.Reset();
  rayProbeRootSignature.Reset();
  probeAuditRootSignature.Reset();
  probeTraceRootSignature.Reset();
  probeDebugRootSignature.Reset();
  stylizeRootSignature.Reset();
  reflectionRootSignature.Reset();
  occlusionRootSignature.Reset();
  focusRootSignature.Reset();
}
} // namespace kuki
#endif
