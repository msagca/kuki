#include <dx_pipeline.hpp>
#ifdef KUKI_HAS_DIRECTX
#include <embedded_shaders.hpp>
#endif
#ifdef KUKI_HAS_DIRECTX
#include <functional>
#include <primitive.hpp>
#include <spdlog/spdlog.h>
#include <string>
namespace kuki {
namespace {
auto MakeKey(const DXGI_FORMAT format, const uint32_t samples) -> uint64_t {
  return (static_cast<uint64_t>(format) << 32) | samples;
}
auto ToBytecode(const ComPtr<IDxcBlob> &blob) -> D3D12_SHADER_BYTECODE {
  return CD3DX12_SHADER_BYTECODE(blob->GetBufferPointer(), blob->GetBufferSize());
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
  CD3DX12_ROOT_PARAMETER parameters[12]{};
  parameters[0].InitAsConstants(sizeof(DXSceneConstants) / sizeof(uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
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
    spdlog::error("[DX12] failed to serialise the scene root signature: {}", log);
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
    return it->second ? &it->second : nullptr;
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetSceneRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = sceneRootSignature;
  const auto vertexShader = shaderCompiler.Compile(embedded_shader::scene_hlsl, skinned ? "VSSkinned" : "VSMain", "vs_6_0", "scene");
  const auto pixelShader = shaderCompiler.Compile(embedded_shader::scene_hlsl, "PSMain", "ps_6_0", "scene");
  if (!vertexShader || !pixelShader)
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
  desc.VS = ToBytecode(vertexShader);
  desc.PS = ToBytecode(pixelShader);
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
  if (DXFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline.pipelineState)), "CreateGraphicsPipelineState"))
    return nullptr;
  spdlog::info("[DX12] created {} {} scene pipeline (format {}, {}x MSAA)", skinned ? "skinned" : "static", blended ? "blended" : "opaque", static_cast<int>(format), samples);
  return &pipeline;
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
    spdlog::error("[DX12] failed to serialise the post root signature: {}", log);
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
    return it->second ? &it->second : nullptr;
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetPostRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = postRootSignature;
  const auto vertexShader = shaderCompiler.Compile(embedded_shader::post_hlsl, "VSMain", "vs_6_0", "post");
  const auto pixelShader = shaderCompiler.Compile(embedded_shader::post_hlsl, effect, "ps_6_0", "post");
  if (!vertexShader || !pixelShader)
    return nullptr;
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.VS = ToBytecode(vertexShader);
  desc.PS = ToBytecode(pixelShader);
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
  if (DXFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline.pipelineState)), "CreateGraphicsPipelineState"))
    return nullptr;
  spdlog::info("[DX12] created post pipeline: {}", effect);
  return &pipeline;
}
auto DXPipelineCache::GetOutlineRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (outlineRootSignature)
    return outlineRootSignature.Get();
  CD3DX12_DESCRIPTOR_RANGE sourceRange{};
  sourceRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  CD3DX12_DESCRIPTOR_RANGE idRange{};
  idRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
  CD3DX12_ROOT_PARAMETER parameters[3]{};
  parameters[0].InitAsConstants(sizeof(DXOutlineConstants) / sizeof(uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[1].InitAsDescriptorTable(1, &sourceRange, D3D12_SHADER_VISIBILITY_PIXEL);
  parameters[2].InitAsDescriptorTable(1, &idRange, D3D12_SHADER_VISIBILITY_PIXEL);
  CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(3, parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] failed to serialise the outline root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&outlineRootSignature)), "CreateRootSignature"))
    return nullptr;
  return outlineRootSignature.Get();
}
auto DXPipelineCache::GetOutlinePipeline(ID3D12Device *device, const DXGI_FORMAT format) -> const DXPipeline * {
  if (!device)
    return nullptr;
  const auto key = MakeKey(format, 0) ^ std::hash<std::string>{}("outline");
  if (auto it = pipelines.find(key); it != pipelines.end())
    return it->second ? &it->second : nullptr;
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetOutlineRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = outlineRootSignature;
  const auto vertexShader = shaderCompiler.Compile(embedded_shader::outline_hlsl, "VSMain", "vs_6_0", "outline");
  const auto pixelShader = shaderCompiler.Compile(embedded_shader::outline_hlsl, "PSOutline", "ps_6_0", "outline");
  if (!vertexShader || !pixelShader)
    return nullptr;
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.VS = ToBytecode(vertexShader);
  desc.PS = ToBytecode(pixelShader);
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
  if (DXFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline.pipelineState)), "CreateGraphicsPipelineState"))
    return nullptr;
  spdlog::info("[DX12] created outline pipeline");
  return &pipeline;
}
auto DXPipelineCache::GetShadowRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (shadowRootSignature)
    return shadowRootSignature.Get();
  CD3DX12_ROOT_PARAMETER parameters[2]{};
  parameters[0].InitAsConstants(sizeof(DXShadowConstants) / sizeof(uint32_t), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
  parameters[1].InitAsShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
  CD3DX12_ROOT_SIGNATURE_DESC desc{};
  desc.Init(2, parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
  ComPtr<ID3DBlob> serialized;
  ComPtr<ID3DBlob> errors;
  if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors))) {
    const auto log = errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : std::string("no output");
    spdlog::error("[DX12] failed to serialise the shadow root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&shadowRootSignature)), "CreateRootSignature"))
    return nullptr;
  return shadowRootSignature.Get();
}
auto DXPipelineCache::GetShadowPipeline(ID3D12Device *device) -> const DXPipeline * {
  if (!device)
    return nullptr;
  const auto key = std::hash<std::string>{}("shadow");
  if (auto it = pipelines.find(key); it != pipelines.end())
    return it->second ? &it->second : nullptr;
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetShadowRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = shadowRootSignature;
  const auto vertexShader = shaderCompiler.Compile(embedded_shader::shadow_hlsl, "VSMain", "vs_6_0", "shadow");
  if (!vertexShader)
    return nullptr;
  const D3D12_INPUT_ELEMENT_DESC inputLayout[]{
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, texture), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, tangent), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.InputLayout = {inputLayout, _countof(inputLayout)};
  desc.pRootSignature = rootSignature;
  desc.VS = ToBytecode(vertexShader);
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
  if (DXFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline.pipelineState)), "CreateGraphicsPipelineState"))
    return nullptr;
  spdlog::info("[DX12] created shadow pipeline");
  return &pipeline;
}
auto DXPipelineCache::GetSkyboxRootSignature(ID3D12Device *device) -> ID3D12RootSignature * {
  if (skyboxRootSignature)
    return skyboxRootSignature.Get();
  CD3DX12_DESCRIPTOR_RANGE equirectRange{};
  equirectRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
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
    spdlog::error("[DX12] failed to serialise the skybox root signature: {}", log);
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
    return it->second ? &it->second : nullptr;
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetSkyboxRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = skyboxRootSignature;
  const auto vertexShader = shaderCompiler.Compile(embedded_shader::skybox_hlsl, "VSMain", "vs_6_0", "skybox");
  const auto pixelShader = shaderCompiler.Compile(embedded_shader::skybox_hlsl, "PSMain", "ps_6_0", "skybox");
  if (!vertexShader || !pixelShader)
    return nullptr;
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.VS = ToBytecode(vertexShader);
  desc.PS = ToBytecode(pixelShader);
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
  if (DXFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline.pipelineState)), "CreateGraphicsPipelineState"))
    return nullptr;
  spdlog::info("[DX12] created skybox pipeline (format {}, {}x MSAA)", static_cast<int>(format), samples);
  return &pipeline;
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
    spdlog::error("[DX12] failed to serialise the compute root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature)), "CreateRootSignature"))
    return nullptr;
  return computeRootSignature.Get();
}
auto DXPipelineCache::GetComputePipeline(ID3D12Device *device, const char *entryPoint) -> const DXPipeline * {
  if (!device || !entryPoint)
    return nullptr;
  const auto key = std::hash<std::string>{}(std::string("compute:") + entryPoint);
  if (auto it = pipelines.find(key); it != pipelines.end())
    return it->second ? &it->second : nullptr;
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetComputeRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = computeRootSignature;
  const auto computeShader = shaderCompiler.Compile(embedded_shader::ibl_hlsl, entryPoint, "cs_6_0", "ibl");
  if (!computeShader)
    return nullptr;
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.CS = ToBytecode(computeShader);
  if (DXFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pipeline.pipelineState)), "CreateComputePipelineState"))
    return nullptr;
  spdlog::info("[DX12] created compute pipeline: {}", entryPoint);
  return &pipeline;
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
    spdlog::error("[DX12] failed to serialise the pick root signature: {}", log);
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
    return it->second ? &it->second : nullptr;
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetPickRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = pickRootSignature;
  const auto computeShader = shaderCompiler.Compile(embedded_shader::pick_hlsl, "CSPick", "cs_6_0", "pick");
  if (!computeShader)
    return nullptr;
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.CS = ToBytecode(computeShader);
  if (DXFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pipeline.pipelineState)), "CreateComputePipelineState"))
    return nullptr;
  spdlog::info("[DX12] created pick pipeline");
  return &pipeline;
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
    spdlog::error("[DX12] failed to serialise the ray probe root signature: {}", log);
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
    return it->second ? &it->second : nullptr;
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetRayProbeRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = rayProbeRootSignature;
  const auto computeShader = shaderCompiler.Compile(embedded_shader::raytrace_hlsl, "CSProbe", "cs_6_5", "raytrace");
  if (!computeShader)
    return nullptr;
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.CS = ToBytecode(computeShader);
  if (DXFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pipeline.pipelineState)), "CreateComputePipelineState"))
    return nullptr;
  spdlog::info("[DX12] created ray probe pipeline (shader model 6.5, inline raytracing)");
  return &pipeline;
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
    spdlog::error("[DX12] failed to serialise the probe audit root signature: {}", log);
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
    return it->second ? &it->second : nullptr;
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetProbeAuditRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = probeAuditRootSignature;
  const auto computeShader = shaderCompiler.Compile(embedded_shader::probe_hlsl, "CSAudit", "cs_6_0", "probe");
  if (!computeShader)
    return nullptr;
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.CS = ToBytecode(computeShader);
  if (DXFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pipeline.pipelineState)), "CreateComputePipelineState"))
    return nullptr;
  spdlog::info("[DX12] created probe audit pipeline");
  return &pipeline;
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
    spdlog::error("[DX12] failed to serialise the probe trace root signature: {}", log);
    return nullptr;
  }
  if (DXFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&probeTraceRootSignature)), "CreateRootSignature"))
    return nullptr;
  return probeTraceRootSignature.Get();
}
auto DXPipelineCache::GetProbeTracePipeline(ID3D12Device *device) -> const DXPipeline * {
  if (!device)
    return nullptr;
  const auto key = std::hash<std::string>{}("probe_trace:CSTrace");
  if (auto it = pipelines.find(key); it != pipelines.end())
    return it->second ? &it->second : nullptr;
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetProbeTraceRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = probeTraceRootSignature;
  const auto computeShader = shaderCompiler.Compile(embedded_shader::probe_trace_hlsl, "CSTrace", "cs_6_5", "probe_trace");
  if (!computeShader)
    return nullptr;
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
  desc.pRootSignature = rootSignature;
  desc.CS = ToBytecode(computeShader);
  if (DXFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pipeline.pipelineState)), "CreateComputePipelineState"))
    return nullptr;
  spdlog::info("[DX12] created probe trace pipeline (shader model 6.5, inline raytracing, bindless)");
  return &pipeline;
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
    spdlog::error("[DX12] failed to serialise the probe debug root signature: {}", log);
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
    return it->second ? &it->second : nullptr;
  auto &pipeline = pipelines[key];
  auto *rootSignature = GetProbeDebugRootSignature(device);
  if (!rootSignature)
    return nullptr;
  pipeline.rootSignature = probeDebugRootSignature;
  const auto vertexShader = shaderCompiler.Compile(embedded_shader::probe_debug_hlsl, "VSMain", "vs_6_0", "probe_debug");
  const auto pixelShader = shaderCompiler.Compile(embedded_shader::probe_debug_hlsl, "PSMain", "ps_6_0", "probe_debug");
  if (!vertexShader || !pixelShader)
    return nullptr;
  const D3D12_INPUT_ELEMENT_DESC inputLayout[]{
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
  };
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  desc.InputLayout = {inputLayout, _countof(inputLayout)};
  desc.pRootSignature = rootSignature;
  desc.VS = ToBytecode(vertexShader);
  desc.PS = ToBytecode(pixelShader);
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
  if (DXFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline.pipelineState)), "CreateGraphicsPipelineState"))
    return nullptr;
  spdlog::info("[DX12] created probe debug pipeline (format {}, {}x MSAA)", static_cast<int>(format), samples);
  return &pipeline;
}
auto DXPipelineCache::Clear() -> void {
  pipelines.clear();
  sceneRootSignature.Reset();
  postRootSignature.Reset();
  outlineRootSignature.Reset();
  shadowRootSignature.Reset();
  skyboxRootSignature.Reset();
  computeRootSignature.Reset();
  pickRootSignature.Reset();
  rayProbeRootSignature.Reset();
  probeAuditRootSignature.Reset();
  probeTraceRootSignature.Reset();
  probeDebugRootSignature.Reset();
}
} // namespace kuki
#endif
