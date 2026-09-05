#include <dx_renderer.hpp>
#ifdef KUKI_HAS_DIRECTX
#include <algorithm>
#include <application.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <dx_context.hpp>
#include <dx_format.hpp>
#include <limits>
#include <hash_utils.hpp>
#include <post_process.hpp>
#include <profiler.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <material_asset.hpp>
#include <mesh_asset.hpp>
#include <light.hpp>
#include <material_handle.hpp>
#include <mesh_handle.hpp>
#include <texture.hpp>
#include <texture_asset.hpp>
#include <variant>
#include <primitive.hpp>
#include <scene.hpp>
#include <skeleton.hpp>
#include <skybox_handle.hpp>
#include <transform.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>
namespace kuki {
namespace {
constexpr float TARGET_CLEAR_COLOR[4]{.05f, .05f, .06f, 1.f};
constexpr uint32_t COMPUTE_STAGING_CAPACITY = 256;
constexpr uint32_t COMPUTE_GROUP_SIZE = 8;
constexpr uint32_t SKYBOX_CUBE_SIZE = 1024;
constexpr uint32_t SKYBOX_CUBE_MIPS = 11;
constexpr uint32_t IRRADIANCE_SIZE = 64;
constexpr uint32_t PREFILTER_SIZE = 256;
constexpr uint32_t PREFILTER_MIPS = 7;
constexpr uint32_t BRDF_SIZE = 512;
constexpr uint32_t SH_FACE_SIZE = 32;
auto GroupCount(const uint32_t extent) -> UINT {
  return static_cast<UINT>((extent + COMPUTE_GROUP_SIZE - 1) / COMPUTE_GROUP_SIZE);
}
/// @brief Folds a run of four-byte fields into a hash, whatever they happen to mean.
///
/// The things worth hashing here are blocks of floats and unsigned integers laid out for a shader,
/// which is to say arrays of four-byte fields with nothing between them. Reading them as words
/// rather than as their own types is what lets one line stand in for a whole light array.
auto HashWords(size_t &hash, const void *data, const size_t bytes) -> void {
  const auto *cursor = static_cast<const uint8_t *>(data);
  for (size_t offset = 0; offset + sizeof(uint32_t) <= bytes; offset += sizeof(uint32_t)) {
    uint32_t word{};
    memcpy(&word, cursor + offset, sizeof(word));
    hash_combine(hash, word);
  }
}
/// @brief Hashes the part of the frame constants the probe trace shades its ray hits from.
///
/// Deliberately not the whole struct. The camera is in there, and a probe's accumulated estimate
/// must survive the camera moving: what arrives at a probe does not depend on who is looking, and
/// restarting on every mouse movement would leave the field permanently unconverged — the exact
/// thing the accumulation exists to avoid. The shadow matrices are left out for the same reason,
/// since the trace shadows itself with rays and the directional one is fitted to the view anyway.
auto HashLighting(const DXFrameConstants &constants) -> size_t {
  constexpr auto begin = offsetof(DXFrameConstants, directionalDirection);
  // Through the flags, not merely the counts. A skybox arrives asynchronously, so the frame it
  // becomes ready is a frame the light changes completely -- and without the flags in the hash the
  // mean would go on averaging the darkness it had gathered before the sky existed.
  constexpr auto end = offsetof(DXFrameConstants, flags) + sizeof(DXFrameConstants::flags);
  size_t hash{};
  HashWords(hash, reinterpret_cast<const uint8_t *>(&constants) + begin, end - begin);
  return hash;
}
/// @brief Hashes the reconstruction values the trace also reads, so moving one resettles the field.
///
/// Its own hash rather than an extension of the range above, because the vector next to this one --
/// how much of each indirect term reaches the picture -- must not be in it. That vector is applied
/// after the field is sampled and changes nothing that was gathered, so folding it in would throw a
/// converged field away every time somebody dragged a brightness slider. These two are different:
/// the trace samples the field for second-bounce light, and it samples it with exactly these, so a
/// change here really is a change to what the probes will hold.
auto HashProbeTuning(const DXFrameConstants &constants) -> size_t {
  size_t hash{};
  HashWords(hash, reinterpret_cast<const uint8_t *>(&constants.probeTuning), sizeof(constants.probeTuning));
  return hash;
}
} // namespace
DXRenderer::DXRenderer(Application &app)
  : Renderer(std::in_place_type<DXRenderer>, app) {}
auto DXRenderer::GetContext() const -> DXContext * {
  return dynamic_cast<DXContext *>(app.GetGraphicsContext());
}
auto DXRenderer::AllocateTarget(DXRenderTarget &target, const TargetDescription &desc, const std::string &name) -> bool {
  auto context = GetContext();
  if (!context || !context->GetDevice())
    return false;
  auto *device = context->GetDevice();
  ReleaseTarget(target);
  const auto depth = IsDepthFormat(desc.format);
  const auto width = static_cast<UINT64>(desc.width > 0 ? desc.width : 1);
  const auto height = static_cast<UINT>(desc.height > 0 ? desc.height : 1);
  const auto layers = static_cast<UINT16>(desc.type == TargetType::Cubemap ? 6 : (desc.layers > 0 ? desc.layers : 1));
  auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(TargetFormatToDXGI(desc.format), width, height, layers, static_cast<UINT16>(desc.mipmaps > 0 ? desc.mipmaps : 1), static_cast<UINT>(desc.samples > 0 ? desc.samples : 1), 0, depth ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
  D3D12_CLEAR_VALUE clearValue{};
  clearValue.Format = TargetFormatToRTVDXGI(desc.format);
  if (depth) {
    clearValue.DepthStencil.Depth = 1.f;
    clearValue.DepthStencil.Stencil = 0;
  } else
    for (auto i = 0; i < 4; ++i)
      clearValue.Color[i] = TARGET_CLEAR_COLOR[i];
  const auto initialState = depth ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET;
  const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  if (DXFailed(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, initialState, &clearValue, IID_PPV_ARGS(&target.resource)), "CreateCommittedResource for target " + name))
    return false;
  target.state = initialState;
  target.desc = desc;
  auto &srvHeap = context->GetSRVHeap();
  target.srvIndex = srvHeap.Allocate();
  if (target.srvIndex != DXDescriptorHeap::InvalidIndex) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = TargetFormatToSRVDXGI(desc.format);
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    if (desc.type == TargetType::Cubemap) {
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
      srvDesc.TextureCube.MipLevels = static_cast<UINT>(desc.mipmaps > 0 ? desc.mipmaps : 1);
    } else if (desc.type == TargetType::Texture2DArray) {
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
      srvDesc.Texture2DArray.MipLevels = static_cast<UINT>(desc.mipmaps > 0 ? desc.mipmaps : 1);
      srvDesc.Texture2DArray.ArraySize = layers;
    } else if (desc.samples > 1)
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    else {
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      srvDesc.Texture2D.MipLevels = static_cast<UINT>(desc.mipmaps > 0 ? desc.mipmaps : 1);
    }
    device->CreateShaderResourceView(target.resource.Get(), &srvDesc, srvHeap.GetCPUHandle(target.srvIndex));
    target.srvGPU = srvHeap.GetGPUHandle(target.srvIndex);
  }
  if (depth) {
    auto &dsvHeap = context->GetDSVHeap();
    const auto arrayed = desc.type == TargetType::Texture2DArray || desc.type == TargetType::Cubemap;
    target.dsvIndex = dsvHeap.Allocate();
    if (target.dsvIndex != DXDescriptorHeap::InvalidIndex) {
      D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
      dsvDesc.Format = TargetFormatToRTVDXGI(desc.format);
      if (arrayed) {
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.ArraySize = layers;
      } else
        dsvDesc.ViewDimension = desc.samples > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
      device->CreateDepthStencilView(target.resource.Get(), &dsvDesc, dsvHeap.GetCPUHandle(target.dsvIndex));
    }
    if (arrayed)
      for (UINT16 layer = 0; layer < layers; ++layer) {
        const auto layerIndex = dsvHeap.Allocate();
        if (layerIndex == DXDescriptorHeap::InvalidIndex)
          break;
        D3D12_DEPTH_STENCIL_VIEW_DESC layerDesc{};
        layerDesc.Format = TargetFormatToRTVDXGI(desc.format);
        layerDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        layerDesc.Texture2DArray.FirstArraySlice = layer;
        layerDesc.Texture2DArray.ArraySize = 1;
        device->CreateDepthStencilView(target.resource.Get(), &layerDesc, dsvHeap.GetCPUHandle(layerIndex));
        target.layerDsvIndices.push_back(layerIndex);
      }
  } else {
    auto &rtvHeap = context->GetRTVHeap();
    target.rtvIndex = rtvHeap.Allocate();
    if (target.rtvIndex != DXDescriptorHeap::InvalidIndex) {
      D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
      rtvDesc.Format = TargetFormatToRTVDXGI(desc.format);
      rtvDesc.ViewDimension = desc.samples > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
      device->CreateRenderTargetView(target.resource.Get(), &rtvDesc, rtvHeap.GetCPUHandle(target.rtvIndex));
    }
  }
  if (desc.pickingBuffer && !depth) {
    const auto samples = static_cast<UINT>(desc.samples > 0 ? desc.samples : 1);
    auto idDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1, samples, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    D3D12_CLEAR_VALUE idClear{};
    idClear.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    if (DXFailed(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &idDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &idClear, IID_PPV_ARGS(&target.idResource)), "CreateCommittedResource for entity id buffer " + name))
      return false;
    auto &idRtvHeap = context->GetRTVHeap();
    target.idRtvIndex = idRtvHeap.Allocate();
    if (target.idRtvIndex != DXDescriptorHeap::InvalidIndex) {
      D3D12_RENDER_TARGET_VIEW_DESC idRtv{};
      idRtv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      idRtv.ViewDimension = samples > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
      device->CreateRenderTargetView(target.idResource.Get(), &idRtv, idRtvHeap.GetCPUHandle(target.idRtvIndex));
    }
    target.idSrvIndex = srvHeap.Allocate();
    if (target.idSrvIndex != DXDescriptorHeap::InvalidIndex) {
      D3D12_SHADER_RESOURCE_VIEW_DESC idSrv{};
      idSrv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      idSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      if (samples > 1)
        idSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
      else {
        idSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        idSrv.Texture2D.MipLevels = 1;
      }
      device->CreateShaderResourceView(target.idResource.Get(), &idSrv, srvHeap.GetCPUHandle(target.idSrvIndex));
      target.idSrvGPU = srvHeap.GetGPUHandle(target.idSrvIndex);
    }
    spdlog::debug("[DXRenderer] created entity id buffer for {} ({}x MSAA)", name, samples);
  }
  spdlog::debug("[DXRenderer] created render target: {} ({}x{})", name, desc.width, desc.height);
  return true;
}
auto DXRenderer::ReleaseTarget(DXRenderTarget &target) -> void {
  auto context = GetContext();
  if (context) {
    context->GetSRVHeap().Free(target.srvIndex);
    context->GetRTVHeap().Free(target.rtvIndex);
    context->GetDSVHeap().Free(target.dsvIndex);
    context->GetSRVHeap().Free(target.idSrvIndex);
    context->GetRTVHeap().Free(target.idRtvIndex);
  }
  target.srvIndex = DXDescriptorHeap::InvalidIndex;
  target.rtvIndex = DXDescriptorHeap::InvalidIndex;
  target.dsvIndex = DXDescriptorHeap::InvalidIndex;
  target.idSrvIndex = DXDescriptorHeap::InvalidIndex;
  target.idRtvIndex = DXDescriptorHeap::InvalidIndex;
  if (context) {
    context->GetDSVHeap().Free(target.depthDsvIndex);
    for (const auto layerIndex : target.layerDsvIndices)
      context->GetDSVHeap().Free(layerIndex);
  }
  target.layerDsvIndices.clear();
  target.depthDsvIndex = DXDescriptorHeap::InvalidIndex;
  target.srvGPU = {};
  target.idSrvGPU = {};
  target.resource.Reset();
  target.idResource.Reset();
  target.depthResource.Reset();
}
auto DXRenderer::Transition(DXRenderTarget &target, const D3D12_RESOURCE_STATES state) -> void {
  auto context = GetContext();
  if (!context || !target.resource || target.state == state)
    return;
  auto *commandList = context->GetCommandList();
  if (!commandList)
    return;
  const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(target.resource.Get(), target.state, state);
  commandList->ResourceBarrier(1, &barrier);
  target.state = state;
}
auto DXRenderer::ClearOutputs(std::span<std::string> outputs) -> void {
  auto context = GetContext();
  if (!context)
    return;
  auto *commandList = context->GetCommandList();
  if (!commandList)
    return;
  for (const auto &name : outputs) {
    auto it = nameToTarget.find(name);
    if (it == nameToTarget.end() || !it->second.resource)
      continue;
    auto &target = it->second;
    const auto depth = IsDepthFormat(target.desc.format);
    Transition(target, depth ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET);
    if (depth) {
      if (target.dsvIndex == DXDescriptorHeap::InvalidIndex)
        continue;
      const auto dsv = context->GetDSVHeap().GetCPUHandle(target.dsvIndex);
      commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
      commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
    } else {
      if (target.rtvIndex == DXDescriptorHeap::InvalidIndex)
        continue;
      const auto rtv = context->GetRTVHeap().GetCPUHandle(target.rtvIndex);
      commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
      commandList->ClearRenderTargetView(rtv, TARGET_CLEAR_COLOR, 0, nullptr);
    }
    const auto viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<float>(target.desc.width), static_cast<float>(target.desc.height));
    const auto scissor = CD3DX12_RECT(0, 0, static_cast<LONG>(target.desc.width), static_cast<LONG>(target.desc.height));
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);
  }
}
auto DXRenderer::BypassPass(const RenderPass pass, std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  // The count is read by the scene pass to decide how many spot shadow matrices to send, and the
  // pass that sets it is the one being stood down, so it would otherwise keep last frame's figure
  // and have the shading sample a map that was cleared out from under it.
  if (pass == RenderPass::SpotShadowMap)
    spotShadowCount = 0;
  Renderer::BypassPass(pass, inputs, outputs);
}
auto DXRenderer::BypassCopy(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  // The first resolved colour input, falling back to a multisampled one only if that is all there
  // is. Order matters twice over: bloom is handed the scene and then the blur of its bright parts,
  // and the scene is the one to pass through; the outline pass is handed the raw multisampled scene
  // and then the composited image, and the composited one is what the passes after it expect.
  const std::string *source{};
  for (const auto &name : inputs) {
    auto it = nameToTarget.find(name);
    if (it == nameToTarget.end() || !it->second.resource || IsDepthFormat(it->second.desc.format))
      continue;
    if (it->second.desc.samples <= 1) {
      source = &name;
      break;
    }
    if (!source)
      source = &name;
  }
  if (!source) {
    ClearOutputs(outputs);
    return;
  }
  std::array<std::string, 1> chosen{*source};
  BlitOrResolve(chosen, outputs);
}
auto DXRenderer::BypassClear(std::span<std::string> outputs) -> void {
  ClearOutputs(outputs);
}
auto DXRenderer::EnsureDepthBuffer(DXRenderTarget &target) -> bool {
  auto context = GetContext();
  if (!context || !target.resource)
    return false;
  if (target.depthResource)
    return true;
  auto *device = context->GetDevice();
  auto desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, static_cast<UINT64>(target.desc.width), static_cast<UINT>(target.desc.height), 1, 1, static_cast<UINT>(target.desc.samples > 0 ? target.desc.samples : 1), 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
  D3D12_CLEAR_VALUE clearValue{};
  clearValue.Format = DXGI_FORMAT_D32_FLOAT;
  clearValue.DepthStencil.Depth = 1.f;
  const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  if (DXFailed(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&target.depthResource)), "CreateCommittedResource for depth"))
    return false;
  target.depthDsvIndex = context->GetDSVHeap().Allocate();
  if (target.depthDsvIndex == DXDescriptorHeap::InvalidIndex)
    return false;
  D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
  dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
  dsvDesc.ViewDimension = target.desc.samples > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
  device->CreateDepthStencilView(target.depthResource.Get(), &dsvDesc, context->GetDSVHeap().GetCPUHandle(target.depthDsvIndex));
  return true;
}
auto DXRenderer::UploadMeshData(const Mesh &source, const std::string &name) -> DXMesh {
  DXMesh mesh;
  auto context = GetContext();
  if (!context || source.vertices.empty())
    return mesh;
  auto *device = context->GetDevice();
  const auto &vertices = source.vertices;
  const auto vertexBytes = static_cast<UINT>(vertices.size() * sizeof(Vertex));
  const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBytes);
  void *mapped{};
  const CD3DX12_RANGE readRange(0, 0);
  if (DXFailed(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mesh.vertexBuffer)), "CreateCommittedResource for vertex buffer"))
    return mesh;
  if (DXFailed(mesh.vertexBuffer->Map(0, &readRange, &mapped), "Map vertex buffer"))
    return mesh;
  memcpy(mapped, vertices.data(), vertexBytes);
  mesh.vertexBuffer->Unmap(0, nullptr);
  mesh.vertexCount = static_cast<uint32_t>(vertices.size());
  mesh.vertexBufferView.BufferLocation = mesh.vertexBuffer->GetGPUVirtualAddress();
  mesh.vertexBufferView.SizeInBytes = vertexBytes;
  mesh.vertexBufferView.StrideInBytes = sizeof(Vertex);
  const auto &indices = source.indices;
  if (!indices.empty()) {
    const auto indexBytes = static_cast<UINT>(indices.size() * sizeof(unsigned int));
    auto indexDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBytes);
    if (!DXFailed(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &indexDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mesh.indexBuffer)), "CreateCommittedResource for index buffer"))
      if (!DXFailed(mesh.indexBuffer->Map(0, &readRange, &mapped), "Map index buffer")) {
        memcpy(mapped, indices.data(), indexBytes);
        mesh.indexBuffer->Unmap(0, nullptr);
        mesh.indexCount = static_cast<uint32_t>(indices.size());
        mesh.indexBufferView.BufferLocation = mesh.indexBuffer->GetGPUVirtualAddress();
        mesh.indexBufferView.SizeInBytes = indexBytes;
        mesh.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
      }
  }
  spdlog::info("[DXRenderer] uploaded mesh: {} ({} vertices)", name, mesh.vertexCount);
  return mesh;
}
auto DXRenderer::EnsureMesh(const AssetID assetId) -> DXMesh * {
  if (auto it = assetToMesh.find(assetId); it != assetToMesh.end())
    return it->second ? &it->second : nullptr;
  auto meshAsset = app.GetAsset<MeshAsset>(assetId);
  if (!meshAsset)
    return nullptr;
  auto &mesh = assetToMesh[assetId];
  mesh = UploadMeshData(meshAsset->mesh, app.GetAssetName(assetId));
  return mesh ? &mesh : nullptr;
}
auto DXRenderer::EnsureModelMesh(const AssetID modelAssetId, const size_t meshIndex) -> DXMesh * {
  auto &meshes = modelToMeshes[modelAssetId];
  if (auto it = meshes.find(meshIndex); it != meshes.end())
    return it->second ? &it->second : nullptr;
  auto modelAsset = app.GetAsset<ModelAsset>(modelAssetId);
  if (!modelAsset || meshIndex >= modelAsset->meshes.size())
    return nullptr;
  auto &mesh = meshes[meshIndex];
  mesh = UploadMeshData(modelAsset->meshes[meshIndex].mesh, modelAsset->meshes[meshIndex].name);
  mesh.skinned = !modelAsset->meshes[meshIndex].bones.empty();
  return mesh ? &mesh : nullptr;
}
auto DXRenderer::UploadTextureData(const Texture &source, const std::string &name) -> DXTexture {
  DXTexture texture;
  auto context = GetContext();
  if (!context)
    return texture;
  if (source.width <= 0 || source.height <= 0)
    return texture;
  if (source.compression != TextureCompression::None)
    return UploadCompressedTextureData(source, name);
  const auto channels = source.channels;
  if (channels != 4 && channels != 3) {
    spdlog::warn("[DXRenderer] unsupported channel count {} for texture {}", channels, name);
    return texture;
  }
  const auto pixelCount = static_cast<size_t>(source.width) * source.height;
  const auto *bytes = std::get_if<std::vector<unsigned char>>(&source.data);
  const auto *floats = std::get_if<std::vector<float>>(&source.data);
  const auto floatingPoint = floats != nullptr;
  if ((!bytes || bytes->empty()) && (!floats || floats->empty()))
    return texture;
  std::vector<unsigned char> expandedBytes;
  std::vector<float> expandedFloats;
  const void *pixels{};
  if (floatingPoint) {
    pixels = floats->data();
    if (channels == 3) {
      expandedFloats.resize(pixelCount * 4, 1.f);
      for (size_t i = 0; i < pixelCount; ++i) {
        expandedFloats[i * 4 + 0] = (*floats)[i * 3 + 0];
        expandedFloats[i * 4 + 1] = (*floats)[i * 3 + 1];
        expandedFloats[i * 4 + 2] = (*floats)[i * 3 + 2];
      }
      pixels = expandedFloats.data();
    }
  } else {
    pixels = bytes->data();
    if (channels == 3) {
      expandedBytes.resize(pixelCount * 4, 255);
      for (size_t i = 0; i < pixelCount; ++i) {
        expandedBytes[i * 4 + 0] = (*bytes)[i * 3 + 0];
        expandedBytes[i * 4 + 1] = (*bytes)[i * 3 + 1];
        expandedBytes[i * 4 + 2] = (*bytes)[i * 3 + 2];
      }
      pixels = expandedBytes.data();
    }
  }
  auto *device = context->GetDevice();
  texture.content = source.content;
  texture.mipLevels = 1;
  const auto bytesPerPixel = static_cast<LONG_PTR>(floatingPoint ? 4 * static_cast<LONG_PTR>(sizeof(float)) : 4);
  const auto format = floatingPoint ? DXGI_FORMAT_R32G32B32A32_FLOAT : (source.color == ColorSpace::sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM);
  auto desc = CD3DX12_RESOURCE_DESC::Tex2D(format, static_cast<UINT64>(source.width), static_cast<UINT>(source.height), 1, 1);
  const auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  if (DXFailed(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture.resource)), "CreateCommittedResource for texture"))
    return texture;
  const auto uploadSize = GetRequiredIntermediateSize(texture.resource.Get(), 0, 1);
  ComPtr<ID3D12Resource> staging;
  const auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
  if (DXFailed(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&staging)), "CreateCommittedResource for texture staging")) {
    texture.resource.Reset();
    return texture;
  }
  auto *commandList = context->GetCommandList();
  if (!commandList) {
    texture.resource.Reset();
    return texture;
  }
  D3D12_SUBRESOURCE_DATA subresource{};
  subresource.pData = pixels;
  subresource.RowPitch = static_cast<LONG_PTR>(source.width) * bytesPerPixel;
  subresource.SlicePitch = subresource.RowPitch * source.height;
  UpdateSubresources<1>(commandList, texture.resource.Get(), staging.Get(), 0, 0, 1, &subresource);
  const auto toShaderResource = CD3DX12_RESOURCE_BARRIER::Transition(texture.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  commandList->ResourceBarrier(1, &toShaderResource);
  RetireStaging(std::move(staging), static_cast<size_t>(uploadSize));
  auto &srvHeap = context->GetSRVHeap();
  texture.srvIndex = srvHeap.Allocate();
  if (texture.srvIndex == DXDescriptorHeap::InvalidIndex) {
    texture.resource.Reset();
    return texture;
  }
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = format;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels = 1;
  device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, srvHeap.GetCPUHandle(texture.srvIndex));
  texture.srvGPU = srvHeap.GetGPUHandle(texture.srvIndex);
  texture.format = format;
  spdlog::info("[DXRenderer] uploaded texture: {} ({}x{})", name, source.width, source.height);
  return texture;
}
/// @brief Uploads an already block-compressed mip chain as one resource of many subresources.
///
/// The chain arrives finished, so this only describes it: one subresource per level, each with the
/// row pitch its block rows imply rather than the pixel rows an uncompressed texture would have.
auto DXRenderer::UploadCompressedTextureData(const Texture &source, const std::string &name) -> DXTexture {
  DXTexture texture;
  auto context = GetContext();
  const auto *blocks = std::get_if<std::vector<unsigned char>>(&source.data);
  const auto levels = GetTextureLevels(source);
  if (!context || !blocks || levels.empty())
    return texture;
  const auto isSRGB = source.color == ColorSpace::sRGB;
  DXGI_FORMAT format;
  auto mapping = static_cast<uint32_t>(D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING);
  switch (source.compression) {
  case TextureCompression::BC1:
    format = isSRGB ? DXGI_FORMAT_BC1_UNORM_SRGB : DXGI_FORMAT_BC1_UNORM;
    break;
  case TextureCompression::BC3:
    format = isSRGB ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM;
    break;
  case TextureCompression::BC4:
    format = DXGI_FORMAT_BC4_UNORM;
    mapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 0, 0, D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1);
    break;
  case TextureCompression::BC5:
    format = DXGI_FORMAT_BC5_UNORM;
    break;
  default:
    spdlog::warn("[DXRenderer] unsupported texture compression for {}", name);
    return texture;
  }
  auto *device = context->GetDevice();
  auto *commandList = context->GetCommandList();
  if (!device || !commandList)
    return texture;
  texture.content = source.content;
  const auto mipLevels = static_cast<UINT16>(levels.size());
  auto desc = CD3DX12_RESOURCE_DESC::Tex2D(format, static_cast<UINT64>(source.width), static_cast<UINT>(source.height), 1, mipLevels);
  const auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  if (DXFailed(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture.resource)), "CreateCommittedResource for compressed texture"))
    return texture;
  std::vector<D3D12_SUBRESOURCE_DATA> subresources(levels.size());
  for (size_t level = 0; level < levels.size(); ++level) {
    const auto &entry = levels[level];
    if (entry.offset + entry.bytes > blocks->size()) {
      spdlog::warn("[DXRenderer] compressed texture {} is shorter than its mip table claims", name);
      texture.resource.Reset();
      return texture;
    }
    const auto blocksHigh = static_cast<LONG_PTR>((entry.height + 3) / 4);
    subresources[level].pData = blocks->data() + entry.offset;
    subresources[level].RowPitch = static_cast<LONG_PTR>(entry.bytes) / blocksHigh;
    subresources[level].SlicePitch = static_cast<LONG_PTR>(entry.bytes);
  }
  const auto uploadSize = GetRequiredIntermediateSize(texture.resource.Get(), 0, mipLevels);
  ComPtr<ID3D12Resource> staging;
  const auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
  if (DXFailed(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&staging)), "CreateCommittedResource for compressed texture staging")) {
    texture.resource.Reset();
    return texture;
  }
  UpdateSubresources(commandList, texture.resource.Get(), staging.Get(), 0, 0, mipLevels, subresources.data());
  const auto toShaderResource = CD3DX12_RESOURCE_BARRIER::Transition(texture.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  commandList->ResourceBarrier(1, &toShaderResource);
  RetireStaging(std::move(staging), static_cast<size_t>(uploadSize));
  auto &srvHeap = context->GetSRVHeap();
  texture.srvIndex = srvHeap.Allocate();
  if (texture.srvIndex == DXDescriptorHeap::InvalidIndex) {
    texture.resource.Reset();
    return texture;
  }
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = format;
  srvDesc.Shader4ComponentMapping = mapping;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels = mipLevels;
  device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, srvHeap.GetCPUHandle(texture.srvIndex));
  texture.srvGPU = srvHeap.GetGPUHandle(texture.srvIndex);
  texture.format = format;
  texture.mipLevels = mipLevels;
  texture.shaderMapping = mapping;
  spdlog::info("[DXRenderer] uploaded compressed texture: {} ({}x{}, {} levels, {} KB)", name, source.width, source.height, mipLevels, blocks->size() / 1024);
  return texture;
}
auto DXRenderer::EnsureUploadBudget() -> void {
  if (uploadBudgetChecked)
    return;
  uploadBudgetChecked = true;
  auto context = GetContext();
  if (!context)
    return;
  const auto heapBudget = context->GetUploadHeapBudget();
  if (!heapBudget)
    return;
  const auto share = static_cast<size_t>(heapBudget / UPLOAD_BUDGET_SHARE);
  const auto adjusted = std::max(MIN_UPLOAD_BUDGET, std::min(uploadBudgetBytes, share));
  if (adjusted == uploadBudgetBytes)
    return;
  spdlog::info("[DXRenderer] upload batch narrowed to {} MB, adapter reports {} MB of shared memory", adjusted / (1024 * 1024), heapBudget / (1024 * 1024));
  uploadBudgetBytes = adjusted;
}
auto DXRenderer::RetireStaging(ComPtr<ID3D12Resource> staging, const size_t bytes) -> void {
  EnsureUploadBudget();
  pendingStaging.push_back(std::move(staging));
  pendingStagingBytes += bytes;
  if (pendingStagingBytes >= uploadBudgetBytes)
    FlushPendingUploads();
}
auto DXRenderer::FlushPendingUploads() -> void {
  if (pendingStaging.empty())
    return;
  KUKI_PROFILE_SCOPE("FlushPendingUploads");
  auto context = GetContext();
  if (!context)
    return;
  context->FlushCommandList();
  spdlog::info("[DXRenderer] uploaded a batch of {} textures ({} MB of staging)", pendingStaging.size(), pendingStagingBytes / (1024 * 1024));
  pendingStaging.clear();
  pendingStagingBytes = 0;
  context->ReleaseRetiredResources();
}
auto DXRenderer::DrainPendingUploads() -> void {
  if (pendingStaging.empty())
    return;
  auto context = GetContext();
  if (!context)
    return;
  for (auto &staging : pendingStaging)
    context->RetireResource(std::move(staging));
  pendingStaging.clear();
  pendingStagingBytes = 0;
}
auto DXRenderer::GetUploadBudget() const -> size_t {
  return uploadBudgetBytes;
}
auto DXRenderer::SetUploadBudget(const size_t bytes) -> void {
  uploadBudgetChecked = true;
  uploadBudgetBytes = std::max(MIN_UPLOAD_BUDGET, bytes);
}
auto DXRenderer::EnsureTexture(const AssetID assetId) -> DXTexture * {
  if (auto it = assetToTexture.find(assetId); it != assetToTexture.end())
    return it->second ? &it->second : nullptr;
  auto textureAsset = app.GetAsset<TextureAsset>(assetId);
  if (!textureAsset)
    return nullptr;
  auto &texture = assetToTexture[assetId];
  texture = UploadTextureData(textureAsset->texture, app.GetAssetName(assetId));
  if (texture)
    ReleaseTexturePixels(textureAsset->texture);
  return texture ? &texture : nullptr;
}
auto DXRenderer::EnsureDummyTexture() -> DXTexture * {
  if (dummyTexture)
    return &dummyTexture;
  Texture white{};
  white.color = ColorSpace::Linear;
  white.content = TextureContent::Albedo;
  white.channels = 4;
  white.width = 1;
  white.height = 1;
  white.data = std::vector<unsigned char>{255, 255, 255, 255};
  dummyTexture = UploadTextureData(white, "DummyWhite");
  if (!dummyTexture)
    return nullptr;
  auto context = GetContext();
  if (context) {
    auto &srvHeap = context->GetSRVHeap();
    dummyArraySrvIndex = srvHeap.Allocate();
    if (dummyArraySrvIndex != DXDescriptorHeap::InvalidIndex) {
      D3D12_SHADER_RESOURCE_VIEW_DESC arrayDesc{};
      arrayDesc.Format = dummyTexture.format;
      arrayDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      arrayDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
      arrayDesc.Texture2DArray.MipLevels = 1;
      arrayDesc.Texture2DArray.ArraySize = 1;
      context->GetDevice()->CreateShaderResourceView(dummyTexture.resource.Get(), &arrayDesc, srvHeap.GetCPUHandle(dummyArraySrvIndex));
      dummyArraySrvGPU = srvHeap.GetGPUHandle(dummyArraySrvIndex);
    }
  }
  if (!fallbackMaterialTable.ptr) {
    BuildMaterialTable(fallbackMaterial, {});
    fallbackMaterialTable.ptr = fallbackMaterial.textureTableGPU;
  }
  return &dummyTexture;
}
auto DXRenderer::BuildMaterialTable(DXMaterial &material, const std::array<const DXTexture *, MATERIAL_TEXTURE_SLOTS> &textures) -> void {
  auto context = GetContext();
  auto fallback = EnsureDummyTexture();
  if (!context || !fallback)
    return;
  auto &srvHeap = context->GetSRVHeap();
  if (!material.textureTableGPU) {
    const auto first = srvHeap.AllocateRange(MATERIAL_TEXTURE_SLOTS);
    if (first == DXDescriptorHeap::InvalidIndex)
      return;
    material.textureTableIndex = first;
    material.textureTableGPU = srvHeap.GetGPUHandle(first).ptr;
  }
  auto *device = context->GetDevice();
  for (uint32_t slot = 0; slot < MATERIAL_TEXTURE_SLOTS; ++slot) {
    const auto *source = textures[slot] && *textures[slot] ? textures[slot] : fallback;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = source->format;
    srvDesc.Shader4ComponentMapping = source->shaderMapping;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = source->mipLevels;
    device->CreateShaderResourceView(source->resource.Get(), &srvDesc, srvHeap.GetCPUHandle(material.textureTableIndex + slot));
  }
}
auto DXRenderer::EnsureModelTexture(const AssetID modelAssetId, const size_t textureIndex) -> DXTexture * {
  auto &textures = modelToTextures[modelAssetId];
  if (auto it = textures.find(textureIndex); it != textures.end())
    return it->second ? &it->second : nullptr;
  auto modelAsset = app.GetAsset<ModelAsset>(modelAssetId);
  if (!modelAsset || textureIndex >= modelAsset->textures.size())
    return nullptr;
  auto &texture = textures[textureIndex];
  texture = UploadTextureData(modelAsset->textures[textureIndex].texture, modelAsset->textures[textureIndex].name);
  if (texture)
    ReleaseTexturePixels(modelAsset->textures[textureIndex].texture);
  return texture ? &texture : nullptr;
}
auto DXRenderer::BlitOrResolve(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  auto context = GetContext();
  if (!context || inputs.empty() || outputs.empty()) {
    ClearOutputs(outputs);
    return;
  }
  auto *commandList = context->GetCommandList();
  auto sourceIt = nameToTarget.find(inputs[0]);
  auto destinationIt = nameToTarget.find(outputs[0]);
  if (!commandList || sourceIt == nameToTarget.end() || destinationIt == nameToTarget.end() || !sourceIt->second.resource || !destinationIt->second.resource) {
    ClearOutputs(outputs);
    return;
  }
  auto &source = sourceIt->second;
  auto &destination = destinationIt->second;
  const auto sourceSamples = source.desc.samples > 0 ? source.desc.samples : 1;
  const auto destinationSamples = destination.desc.samples > 0 ? destination.desc.samples : 1;
  if (source.desc.width != destination.desc.width || source.desc.height != destination.desc.height) {
    ClearOutputs(outputs);
    return;
  }
  if (sourceSamples > 1 && destinationSamples == 1) {
    Transition(source, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    Transition(destination, D3D12_RESOURCE_STATE_RESOLVE_DEST);
    commandList->ResolveSubresource(destination.resource.Get(), 0, source.resource.Get(), 0, TargetFormatToRTVDXGI(destination.desc.format));
  } else if (sourceSamples == destinationSamples) {
    Transition(source, D3D12_RESOURCE_STATE_COPY_SOURCE);
    Transition(destination, D3D12_RESOURCE_STATE_COPY_DEST);
    commandList->CopyResource(destination.resource.Get(), source.resource.Get());
  } else
    ClearOutputs(outputs);
}
auto DXRenderer::ApplyFullscreenEffect(std::span<std::string> inputs, std::span<std::string> outputs, const char *effect, const float parameter, const bool horizontal) -> void {
  auto context = GetContext();
  if (!context || inputs.empty() || outputs.empty()) {
    BlitOrResolve(inputs, outputs);
    return;
  }
  auto *commandList = context->GetCommandList();
  auto sourceIt = nameToTarget.find(inputs[0]);
  auto destinationIt = nameToTarget.find(outputs[0]);
  if (!commandList || sourceIt == nameToTarget.end() || destinationIt == nameToTarget.end() || !sourceIt->second.resource || !destinationIt->second.resource) {
    BlitOrResolve(inputs, outputs);
    return;
  }
  auto &source = sourceIt->second;
  auto &destination = destinationIt->second;
  if (source.desc.samples > 1 || destination.desc.samples > 1 || source.srvIndex == DXDescriptorHeap::InvalidIndex || destination.rtvIndex == DXDescriptorHeap::InvalidIndex) {
    BlitOrResolve(inputs, outputs);
    return;
  }
  const auto pipeline = pipelines.GetPostPipeline(context->GetDevice(), TargetFormatToRTVDXGI(destination.desc.format), effect);
  if (!pipeline || !*pipeline) {
    BlitOrResolve(inputs, outputs);
    return;
  }
  Transition(source, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  Transition(destination, D3D12_RESOURCE_STATE_RENDER_TARGET);
  const auto rtv = context->GetRTVHeap().GetCPUHandle(destination.rtvIndex);
  commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
  const auto viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<float>(destination.desc.width), static_cast<float>(destination.desc.height));
  const auto scissor = CD3DX12_RECT(0, 0, static_cast<LONG>(destination.desc.width), static_cast<LONG>(destination.desc.height));
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  commandList->SetGraphicsRootSignature(pipeline->rootSignature.Get());
  commandList->SetPipelineState(pipeline->pipelineState.Get());
  DXPostConstants constants{};
  constants.parameter = parameter;
  constants.texelSize[0] = destination.desc.width > 0 ? 1.f / destination.desc.width : 0.f;
  constants.texelSize[1] = destination.desc.height > 0 ? 1.f / destination.desc.height : 0.f;
  constants.horizontal = horizontal ? 1u : 0u;
  // Filled for every effect rather than only for the one that maps the image, because the bright
  // pass reads the exposure too, and a root constant nothing looks at costs a dword.
  constants.exposure = exposure;
  constants.toneMapper = static_cast<uint32_t>(toneMapper);
  // Read by the tone mapping and bright pass shaders alone, and filled here with the rest for the
  // same reason they are: a root constant nothing looks at costs a dword.
  constants.raw = lightingDebugView != LightingDebugView::None ? 1u : 0u;
  auto secondGPU = source.srvGPU;
  if (inputs.size() > 1)
    if (auto secondIt = nameToTarget.find(inputs[1]); secondIt != nameToTarget.end() && secondIt->second.resource && secondIt->second.desc.samples <= 1 && secondIt->second.srvIndex != DXDescriptorHeap::InvalidIndex) {
      Transition(secondIt->second, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      secondGPU = secondIt->second.srvGPU;
    }
  commandList->SetGraphicsRoot32BitConstants(0, sizeof(DXPostConstants) / sizeof(uint32_t), &constants, 0);
  commandList->SetGraphicsRootDescriptorTable(1, source.srvGPU);
  commandList->SetGraphicsRootDescriptorTable(2, secondGPU);
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commandList->IASetVertexBuffers(0, 0, nullptr);
  commandList->DrawInstanced(3, 1, 0, 0);
}
auto DXRenderer::CreateComputeTexture(DXComputeTexture &texture, const DXGI_FORMAT format, const uint32_t size, const uint32_t mipLevels, const uint32_t arraySize, const std::string &name) -> bool {
  auto context = GetContext();
  if (!context || !context->GetDevice())
    return false;
  ReleaseComputeTexture(texture);
  auto *device = context->GetDevice();
  auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, size, size, static_cast<UINT16>(arraySize), static_cast<UINT16>(mipLevels), 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  if (DXFailed(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&texture.resource)), "CreateCommittedResource for " + name))
    return false;
  texture.state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  texture.size = size;
  texture.mipLevels = mipLevels;
  texture.arraySize = arraySize;
  auto &srvHeap = computeStagingHeap;
  texture.srvIndex = srvHeap.Allocate();
  if (texture.srvIndex == DXDescriptorHeap::InvalidIndex)
    return false;
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = format;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  if (arraySize == 6) {
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MipLevels = mipLevels;
  } else {
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = mipLevels;
  }
  device->CreateShaderResourceView(texture.resource.Get(), &srvDesc, srvHeap.GetCPUHandle(texture.srvIndex));
  for (uint32_t mip = 0; mip < mipLevels; ++mip) {
    const auto uavIndex = srvHeap.Allocate();
    if (uavIndex == DXDescriptorHeap::InvalidIndex)
      return false;
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = format;
    if (arraySize > 1) {
      uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
      uavDesc.Texture2DArray.MipSlice = mip;
      uavDesc.Texture2DArray.ArraySize = arraySize;
    } else {
      uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
      uavDesc.Texture2D.MipSlice = mip;
    }
    device->CreateUnorderedAccessView(texture.resource.Get(), nullptr, &uavDesc, srvHeap.GetCPUHandle(uavIndex));
    texture.mipUavIndices.push_back(uavIndex);
    const auto mipSrvIndex = srvHeap.Allocate();
    if (mipSrvIndex == DXDescriptorHeap::InvalidIndex)
      return false;
    D3D12_SHADER_RESOURCE_VIEW_DESC mipSrvDesc{};
    mipSrvDesc.Format = format;
    mipSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    mipSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    mipSrvDesc.Texture2DArray.MostDetailedMip = mip;
    mipSrvDesc.Texture2DArray.MipLevels = 1;
    mipSrvDesc.Texture2DArray.ArraySize = arraySize;
    device->CreateShaderResourceView(texture.resource.Get(), &mipSrvDesc, srvHeap.GetCPUHandle(mipSrvIndex));
    texture.mipSrvIndices.push_back(mipSrvIndex);
  }
  spdlog::debug("[DXRenderer] created compute texture: {} ({}x{}, {} mips, {} slices)", name, size, size, mipLevels, arraySize);
  return true;
}
auto DXRenderer::ReleaseComputeTexture(DXComputeTexture &texture) -> void {
  computeStagingHeap.Free(texture.srvIndex);
  for (const auto index : texture.mipUavIndices)
    computeStagingHeap.Free(index);
  for (const auto index : texture.mipSrvIndices)
    computeStagingHeap.Free(index);
  texture.srvIndex = DXDescriptorHeap::InvalidIndex;
  texture.mipUavIndices.clear();
  texture.mipSrvIndices.clear();
  texture.resource.Reset();
  texture.state = D3D12_RESOURCE_STATE_COMMON;
}
auto DXRenderer::TransitionMip(ID3D12Resource *resource, const uint32_t mip, const uint32_t mipLevels, const uint32_t arraySize, const D3D12_RESOURCE_STATES before, const D3D12_RESOURCE_STATES after) -> void {
  auto context = GetContext();
  if (!context || !resource || before == after)
    return;
  auto *commandList = context->GetCommandList();
  if (!commandList)
    return;
  std::vector<D3D12_RESOURCE_BARRIER> barriers;
  barriers.reserve(arraySize);
  for (uint32_t slice = 0; slice < arraySize; ++slice)
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(resource, before, after, D3D12CalcSubresource(mip, slice, 0, mipLevels, arraySize)));
  commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
}
auto DXRenderer::EnsureComputeFallbacks() -> bool {
  auto context = GetContext();
  if (!context || !context->GetDevice())
    return false;
  auto *device = context->GetDevice();
  if (!computeStagingHeap.Get())
    if (!computeStagingHeap.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, COMPUTE_STAGING_CAPACITY))
      return false;
  if (!dummyCubemap)
    if (!CreateComputeTexture(dummyCubemap, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 1, 6, "DummyCubemap"))
      return false;
  if (dummyEquirectSrv == DXDescriptorHeap::InvalidIndex) {
    auto fallback = EnsureDummyTexture();
    if (!fallback)
      return false;
    dummyEquirectSrv = computeStagingHeap.Allocate();
    if (dummyEquirectSrv == DXDescriptorHeap::InvalidIndex)
      return false;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = fallback->format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(fallback->resource.Get(), &srvDesc, computeStagingHeap.GetCPUHandle(dummyEquirectSrv));
  }
  if (shBuffer)
    return true;
  constexpr uint32_t COEFFICIENTS = 9;
  const auto bytes = COEFFICIENTS * sizeof(float) * 4;
  auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
  const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  if (DXFailed(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&shBuffer)), "CreateCommittedResource for spherical harmonics"))
    return false;
  shState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  auto &srvHeap = computeStagingHeap;
  shUavIndex = srvHeap.Allocate();
  shSrvIndex = srvHeap.Allocate();
  if (shUavIndex == DXDescriptorHeap::InvalidIndex || shSrvIndex == DXDescriptorHeap::InvalidIndex)
    return false;
  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
  uavDesc.Format = DXGI_FORMAT_UNKNOWN;
  uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  uavDesc.Buffer.NumElements = COEFFICIENTS;
  uavDesc.Buffer.StructureByteStride = sizeof(float) * 4;
  device->CreateUnorderedAccessView(shBuffer.Get(), nullptr, &uavDesc, srvHeap.GetCPUHandle(shUavIndex));
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = DXGI_FORMAT_UNKNOWN;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  srvDesc.Buffer.NumElements = COEFFICIENTS;
  srvDesc.Buffer.StructureByteStride = sizeof(float) * 4;
  device->CreateShaderResourceView(shBuffer.Get(), &srvDesc, srvHeap.GetCPUHandle(shSrvIndex));
  if (fallbackEnvironmentIndex == DXDescriptorHeap::InvalidIndex) {
    auto &visibleHeap = context->GetSRVHeap();
    fallbackEnvironmentIndex = visibleHeap.AllocateRange(3);
    if (fallbackEnvironmentIndex == DXDescriptorHeap::InvalidIndex)
      return false;
    fallbackEnvironmentTable = visibleHeap.GetGPUHandle(fallbackEnvironmentIndex);
    const uint32_t views[3]{dummyCubemap.srvIndex, dummyCubemap.srvIndex, dummyEquirectSrv};
    for (uint32_t slot = 0; slot < 3; ++slot)
      device->CopyDescriptorsSimple(1, visibleHeap.GetCPUHandle(fallbackEnvironmentIndex + slot), computeStagingHeap.GetCPUHandle(views[slot]), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  }
  if (fallbackSkyboxIndex == DXDescriptorHeap::InvalidIndex) {
    auto &visibleHeap = context->GetSRVHeap();
    fallbackSkyboxIndex = visibleHeap.Allocate();
    if (fallbackSkyboxIndex == DXDescriptorHeap::InvalidIndex)
      return false;
    fallbackSkyboxTable = visibleHeap.GetGPUHandle(fallbackSkyboxIndex);
    device->CopyDescriptorsSimple(1, visibleHeap.GetCPUHandle(fallbackSkyboxIndex), computeStagingHeap.GetCPUHandle(dummyCubemap.srvIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  }
  return true;
}
auto DXRenderer::BindComputeResources(const std::array<uint32_t, COMPUTE_SRV_SLOTS> &sources, const uint32_t destination) -> bool {
  auto context = GetContext();
  if (!context)
    return false;
  auto *commandList = context->GetCommandList();
  auto *device = context->GetDevice();
  if (!commandList || !device)
    return false;
  auto &srvHeap = context->GetSRVHeap();
  const auto block = srvHeap.AllocateRange(COMPUTE_SRV_SLOTS + COMPUTE_UAV_SLOTS);
  if (block == DXDescriptorHeap::InvalidIndex)
    return false;
  const uint32_t fallbacks[COMPUTE_SRV_SLOTS]{dummyEquirectSrv, dummyCubemap.srvIndex, dummyCubemap.mipSrvIndices.front(), shSrvIndex};
  for (uint32_t slot = 0; slot < COMPUTE_SRV_SLOTS; ++slot) {
    const auto source = sources[slot] != DXDescriptorHeap::InvalidIndex ? sources[slot] : fallbacks[slot];
    device->CopyDescriptorsSimple(1, srvHeap.GetCPUHandle(block + slot), computeStagingHeap.GetCPUHandle(source), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  }
  const uint32_t destinations[COMPUTE_UAV_SLOTS]{destination != DXDescriptorHeap::InvalidIndex ? destination : dummyCubemap.mipUavIndices.front(), brdfLUT ? brdfLUT.mipUavIndices.front() : dummyCubemap.mipUavIndices.front(), shUavIndex};
  for (uint32_t slot = 0; slot < COMPUTE_UAV_SLOTS; ++slot)
    device->CopyDescriptorsSimple(1, srvHeap.GetCPUHandle(block + COMPUTE_SRV_SLOTS + slot), computeStagingHeap.GetCPUHandle(destinations[slot]), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  commandList->SetComputeRootDescriptorTable(1, srvHeap.GetGPUHandle(block));
  commandList->SetComputeRootDescriptorTable(2, srvHeap.GetGPUHandle(block + COMPUTE_SRV_SLOTS));
  // Retired here, which is the whole point of retiring rather than freeing. This runs once per
  // dispatch, and preparing one skybox is twenty-one of them: a cubemap, ten downsamples, the
  // harmonics, the irradiance, seven prefiltered roughness levels and the lookup table. Taken from
  // fresh capacity and never given back, that was a hundred and forty-seven descriptors per skybox
  // out of four thousand, so a session that tried a few dozen skyboxes ran the heap dry.
  RetireDescriptorRange(block, COMPUTE_SRV_SLOTS + COMPUTE_UAV_SLOTS);
  return true;
}
auto DXRenderer::BuildEnvironmentMaps(const AssetID assetId, const DXTexture &equirect) -> bool {
  auto context = GetContext();
  if (!context || !equirect)
    return false;
  auto *commandList = context->GetCommandList();
  auto *device = context->GetDevice();
  if (!commandList || !device || !EnsureComputeFallbacks())
    return false;
  environmentReady = false;
  if (skyboxCubemap)
    context->WaitForGPU();
  if (!CreateComputeTexture(skyboxCubemap, DXGI_FORMAT_R32G32B32A32_FLOAT, SKYBOX_CUBE_SIZE, SKYBOX_CUBE_MIPS, 6, "SkyboxCubemap"))
    return false;
  if (!CreateComputeTexture(irradianceMap, DXGI_FORMAT_R32G32B32A32_FLOAT, IRRADIANCE_SIZE, 1, 6, "IrradianceMap"))
    return false;
  if (!CreateComputeTexture(prefilterMap, DXGI_FORMAT_R32G32B32A32_FLOAT, PREFILTER_SIZE, PREFILTER_MIPS, 6, "PrefilterMap"))
    return false;
  if (!CreateComputeTexture(brdfLUT, DXGI_FORMAT_R16G16_FLOAT, BRDF_SIZE, 1, 1, "BRDF_LUT"))
    return false;
  auto equirectSrv = computeStagingHeap.Allocate();
  if (equirectSrv == DXDescriptorHeap::InvalidIndex)
    return false;
  D3D12_SHADER_RESOURCE_VIEW_DESC equirectDesc{};
  equirectDesc.Format = equirect.format;
  equirectDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  equirectDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  equirectDesc.Texture2D.MipLevels = 1;
  device->CreateShaderResourceView(equirect.resource.Get(), &equirectDesc, computeStagingHeap.GetCPUHandle(equirectSrv));
  const auto equirectToRead = CD3DX12_RESOURCE_BARRIER::Transition(equirect.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  commandList->ResourceBarrier(1, &equirectToRead);
  ID3D12DescriptorHeap *heaps[]{context->GetSRVHeap().Get()};
  commandList->SetDescriptorHeaps(1, heaps);
  const auto Dispatch = [&](const char *entryPoint, const DXIBLConstants &constants, const std::array<uint32_t, COMPUTE_SRV_SLOTS> &sources, const uint32_t destination, const UINT x, const UINT y, const UINT z) -> bool {
    const auto pipeline = pipelines.GetComputePipeline(device, entryPoint);
    if (!pipeline || !*pipeline)
      return false;
    commandList->SetComputeRootSignature(pipeline->rootSignature.Get());
    commandList->SetPipelineState(pipeline->pipelineState.Get());
    commandList->SetComputeRoot32BitConstants(0, sizeof(DXIBLConstants) / sizeof(uint32_t), &constants, 0);
    if (!BindComputeResources(sources, destination))
      return false;
    commandList->Dispatch(x, y, z);
    return true;
  };
  const auto Barrier = [&](ID3D12Resource *resource) {
    const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource);
    commandList->ResourceBarrier(1, &barrier);
  };
  constexpr auto INVALID = DXDescriptorHeap::InvalidIndex;
  DXIBLConstants constants{};
  constants.size = SKYBOX_CUBE_SIZE;
  if (!Dispatch("CSEquirectToCubemap", constants, {equirectSrv, INVALID, INVALID, INVALID}, skyboxCubemap.mipUavIndices[0], GroupCount(SKYBOX_CUBE_SIZE), GroupCount(SKYBOX_CUBE_SIZE), 6))
    return false;
  Barrier(skyboxCubemap.resource.Get());
  for (uint32_t mip = 1; mip < SKYBOX_CUBE_MIPS; ++mip) {
    TransitionMip(skyboxCubemap.resource.Get(), mip - 1, SKYBOX_CUBE_MIPS, 6, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    DXIBLConstants mipConstants{};
    mipConstants.size = std::max(SKYBOX_CUBE_SIZE >> mip, 1u);
    if (!Dispatch("CSDownsampleCubemap", mipConstants, {INVALID, INVALID, skyboxCubemap.mipSrvIndices[mip - 1], INVALID}, skyboxCubemap.mipUavIndices[mip], GroupCount(mipConstants.size), GroupCount(mipConstants.size), 6))
      return false;
    Barrier(skyboxCubemap.resource.Get());
  }
  TransitionMip(skyboxCubemap.resource.Get(), SKYBOX_CUBE_MIPS - 1, SKYBOX_CUBE_MIPS, 6, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  skyboxCubemap.state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  DXIBLConstants shConstants{};
  shConstants.faceSize = SH_FACE_SIZE;
  shConstants.mipLevel = std::log2(static_cast<float>(SKYBOX_CUBE_SIZE) / static_cast<float>(SH_FACE_SIZE));
  if (!Dispatch("CSSHProject", shConstants, {INVALID, skyboxCubemap.srvIndex, INVALID, INVALID}, INVALID, 1, 1, 1))
    return false;
  Barrier(shBuffer.Get());
  const auto shToRead = CD3DX12_RESOURCE_BARRIER::Transition(shBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  commandList->ResourceBarrier(1, &shToRead);
  DXIBLConstants irradianceConstants{};
  irradianceConstants.size = IRRADIANCE_SIZE;
  if (!Dispatch("CSIrradiance", irradianceConstants, {INVALID, INVALID, INVALID, shSrvIndex}, irradianceMap.mipUavIndices[0], GroupCount(IRRADIANCE_SIZE), GroupCount(IRRADIANCE_SIZE), 6))
    return false;
  for (uint32_t mip = 0; mip < PREFILTER_MIPS; ++mip) {
    DXIBLConstants prefilterConstants{};
    prefilterConstants.size = std::max(PREFILTER_SIZE >> mip, 1u);
    prefilterConstants.mipLevels = SKYBOX_CUBE_MIPS;
    prefilterConstants.sourceSize = SKYBOX_CUBE_SIZE;
    prefilterConstants.roughness = static_cast<float>(mip) / static_cast<float>(PREFILTER_MIPS - 1);
    if (!Dispatch("CSPrefilter", prefilterConstants, {INVALID, skyboxCubemap.srvIndex, INVALID, INVALID}, prefilterMap.mipUavIndices[mip], GroupCount(prefilterConstants.size), GroupCount(prefilterConstants.size), 6))
      return false;
  }
  DXIBLConstants brdfConstants{};
  brdfConstants.size = BRDF_SIZE;
  if (!Dispatch("CSBRDF", brdfConstants, {INVALID, INVALID, INVALID, INVALID}, INVALID, GroupCount(BRDF_SIZE), GroupCount(BRDF_SIZE), 1))
    return false;
  Barrier(irradianceMap.resource.Get());
  Barrier(prefilterMap.resource.Get());
  Barrier(brdfLUT.resource.Get());
  D3D12_RESOURCE_BARRIER finalBarriers[]{
    CD3DX12_RESOURCE_BARRIER::Transition(skyboxCubemap.resource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
    CD3DX12_RESOURCE_BARRIER::Transition(irradianceMap.resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
    CD3DX12_RESOURCE_BARRIER::Transition(prefilterMap.resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
    CD3DX12_RESOURCE_BARRIER::Transition(brdfLUT.resource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
    CD3DX12_RESOURCE_BARRIER::Transition(equirect.resource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)};
  commandList->ResourceBarrier(_countof(finalBarriers), finalBarriers);
  skyboxCubemap.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  irradianceMap.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  prefilterMap.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  brdfLUT.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  context->FlushCommandList();
  computeStagingHeap.Free(equirectSrv);
  auto &srvHeap = context->GetSRVHeap();
  if (environmentTableIndex == DXDescriptorHeap::InvalidIndex) {
    environmentTableIndex = srvHeap.AllocateRange(3);
    if (environmentTableIndex == DXDescriptorHeap::InvalidIndex)
      return false;
    environmentTable = srvHeap.GetGPUHandle(environmentTableIndex);
  }
  const uint32_t environmentViews[3]{irradianceMap.srvIndex, prefilterMap.srvIndex, brdfLUT.srvIndex};
  for (uint32_t slot = 0; slot < 3; ++slot)
    device->CopyDescriptorsSimple(1, srvHeap.GetCPUHandle(environmentTableIndex + slot), computeStagingHeap.GetCPUHandle(environmentViews[slot]), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  if (skyboxTableIndex == DXDescriptorHeap::InvalidIndex) {
    skyboxTableIndex = srvHeap.Allocate();
    if (skyboxTableIndex == DXDescriptorHeap::InvalidIndex)
      return false;
    skyboxTable = srvHeap.GetGPUHandle(skyboxTableIndex);
  }
  device->CopyDescriptorsSimple(1, srvHeap.GetCPUHandle(skyboxTableIndex), computeStagingHeap.GetCPUHandle(skyboxCubemap.srvIndex), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  environmentAsset = assetId;
  environmentReady = true;
  spdlog::info("[DXRenderer] precomputed image-based lighting for {}", app.GetAssetName(assetId));
  return true;
}
auto DXRenderer::EnsureInstanceCapacity(const uint64_t bytes) -> bool {
  auto context = GetContext();
  if (!context || !context->GetDevice())
    return false;
  if (instanceData && instanceCapacity >= bytes)
    return true;
  auto capacity = instanceCapacity > 0 ? instanceCapacity : 256 * sizeof(DXInstanceData);
  while (capacity < bytes)
    capacity *= 2;
  const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(capacity * DX_FRAME_COUNT);
  ComPtr<ID3D12Resource> grown;
  if (DXFailed(context->GetDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&grown)), "CreateCommittedResource for instances"))
    return false;
  void *mapped{};
  const CD3DX12_RANGE readRange(0, 0);
  if (DXFailed(grown->Map(0, &readRange, &mapped), "Map instances"))
    return false;
  // Growth can land in the middle of a frame -- a preview drawn after the scene pass pushes the
  // cursor further than the pass reserved for. Releasing the old arena here would pull the ground
  // out from under draws already recorded against it, so it is retired instead and the GPU is not
  // waited on at all.
  RetireUploadBuffer(std::move(instanceBuffer));
  instanceBuffer = std::move(grown);
  instanceData = static_cast<uint8_t *>(mapped);
  instanceCapacity = capacity;
  spdlog::debug("[DXRenderer] instance arena grown to {} instances per frame", capacity / sizeof(DXInstanceData));
  return true;
}
auto DXRenderer::AllocateInstances(std::span<const DXInstanceData> instances) -> D3D12_GPU_VIRTUAL_ADDRESS {
  auto context = GetContext();
  if (!context || instances.empty())
    return 0;
  constexpr uint64_t ALIGNMENT = D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT;
  const auto offset = (instanceCursor + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
  const auto bytes = instances.size() * sizeof(DXInstanceData);
  if (!EnsureInstanceCapacity(offset + bytes))
    return 0;
  const auto frameOffset = instanceCapacity * (context->GetFrameIndex() % DX_FRAME_COUNT);
  memcpy(instanceData + frameOffset + offset, instances.data(), bytes);
  instanceCursor = offset + bytes;
  return instanceBuffer->GetGPUVirtualAddress() + frameOffset + offset;
}
auto DXRenderer::AllocateBones(const std::vector<glm::mat4> &bones) -> D3D12_GPU_VIRTUAL_ADDRESS {
  auto context = GetContext();
  if (!context || bones.empty())
    return 0;
  constexpr uint64_t ALIGNMENT = D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT;
  const auto offset = (instanceCursor + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
  const auto bytes = bones.size() * sizeof(glm::mat4);
  if (!EnsureInstanceCapacity(offset + bytes))
    return 0;
  const auto frameOffset = instanceCapacity * (context->GetFrameIndex() % DX_FRAME_COUNT);
  memcpy(instanceData + frameOffset + offset, bones.data(), bytes);
  instanceCursor = offset + bytes;
  return instanceBuffer->GetGPUVirtualAddress() + frameOffset + offset;
}
auto DXRenderer::CollectSkinnedDraws(Scene &scene, const Camera *camera) -> std::vector<DXSkinnedDraw> {
  std::vector<DXSkinnedDraw> draws;
  scene.ForEachEntity<ModelMeshHandle, Transform, Optional<BoundingBox>>([&](const EntityID id, const ModelMeshHandle *handle, const Transform *transform, const BoundingBox *bounds) {
    auto modelIt = modelToMeshes.find(handle->modelAssetId);
    if (modelIt == modelToMeshes.end())
      return;
    auto meshIt = modelIt->second.find(handle->meshIndex);
    if (meshIt == modelIt->second.end() || !meshIt->second || !meshIt->second.skinned)
      return;
    if (camera && bounds && *bounds && !camera->IntersectsFrustum(bounds->GetWorldBounds(transform->world)))
      return;
    auto modelAsset = app.GetAsset<ModelAsset>(handle->modelAssetId);
    if (!modelAsset || handle->meshIndex >= modelAsset->meshes.size())
      return;
    const auto &modelMesh = modelAsset->meshes[handle->meshIndex];
    const Skeleton *skeleton{};
    auto ancestorId = id;
    for (auto depth = 0; depth < 64 && ancestorId; ++depth) {
      if (auto found = scene.GetEntityComponent<Skeleton>(ancestorId); found) {
        skeleton = found;
        break;
      }
      ancestorId = scene.GetParent(ancestorId);
    }
    DXSkinnedDraw draw{};
    draw.mesh = &meshIt->second;
    draw.material = scene.GetEntityComponent<DXMaterial>(id);
    if (!draw.material)
      if (const auto parent = scene.GetParent(id); parent)
        draw.material = scene.GetEntityComponent<DXMaterial>(parent);
    memcpy(draw.instance.model, glm::value_ptr(transform->world), sizeof(draw.instance.model));
    draw.instance.entityId = static_cast<uint32_t>(static_cast<long long>(id)) & 0xFFFFFFu;
    draw.bones.assign(modelMesh.bones.size(), glm::mat4(1.f));
    if (skeleton)
      for (size_t i = 0; i < modelMesh.bones.size(); ++i) {
        const auto &bone = modelMesh.bones[i];
        if (bone.nodeIndex >= skeleton->nodeEntities.size())
          continue;
        const auto boneEntityId = skeleton->nodeEntities[bone.nodeIndex];
        if (!boneEntityId)
          continue;
        if (auto boneTransform = scene.GetEntityComponent<Transform>(boneEntityId); boneTransform)
          draw.bones[i] = boneTransform->world * bone.offsetMatrix;
      }
    draws.push_back(std::move(draw));
  });
  return draws;
}
auto DXBatchKeyHash::operator()(const DXBatchKey &key) const noexcept -> size_t {
  auto seed = std::hash<const DXMesh *>{}(key.mesh);
  hash_combine(seed, std::hash<uint64_t>{}(key.textureTable));
  hash_combine(seed, std::hash<uint32_t>{}(key.variant));
  return seed;
}
auto DXRenderer::CollectBatches(Scene &scene, const bool shadowCastersOnly, const Camera *camera) -> std::span<const DXDrawBatch> {
  KUKI_PROFILE_SCOPE("CollectBatches");
  // How many of the pooled batches this call has claimed; everything past it is last call's, left
  // alone so its capacity survives.
  size_t used = 0;
  batchLookup.clear();
  const auto Add = [&](const DXMesh *mesh, const DXMaterial *material, const Transform *transform, const BoundingBox *bounds, const EntityID entityId) {
    if (!mesh || !*mesh || mesh->skinned)
      return;
    if (shadowCastersOnly && material && (material->fallback.alphaMode == AlphaMode::Blend || material->fallback.transmission > .0f))
      return;
    const auto grouped = !shadowCastersOnly && material;
    const auto blended = grouped && material->fallback.alphaMode == AlphaMode::Blend;
    const auto variant = grouped ? static_cast<uint32_t>(material->type) + 1u + 4u * (static_cast<uint32_t>(material->fallback.alphaMode) + 1u) : 0u;
    const DXBatchKey key{mesh, grouped ? material->textureTableGPU : 0ull, blended ? static_cast<uint32_t>(static_cast<long long>(entityId)) : variant};
    auto it = batchLookup.find(key);
    if (it == batchLookup.end()) {
      it = batchLookup.emplace(key, used).first;
      if (used == batchPool.size()) {
        batchPool.emplace_back();
        hiddenPool.emplace_back();
      }
      auto &claimed = batchPool[used];
      claimed.mesh = mesh;
      claimed.material = shadowCastersOnly ? nullptr : material;
      // clear, never shrink: the capacity is the whole point of pooling these
      claimed.instances.clear();
      claimed.visibleCount = 0;
      hiddenPool[used].clear();
      ++used;
    }
    DXInstanceData instance{};
    memcpy(instance.model, glm::value_ptr(transform->world), sizeof(instance.model));
    instance.entityId = static_cast<uint32_t>(static_cast<long long>(entityId)) & 0xFFFFFFu;
    // an entity with no bounds cannot be tested, so it is drawn; over-drawing is the safe way to be wrong
    const auto visible = !camera || !bounds || !*bounds || camera->IntersectsFrustum(bounds->GetWorldBounds(transform->world));
    if (visible)
      batchPool[it->second].instances.push_back(instance);
    else
      hiddenPool[it->second].push_back(instance);
  };
  scene.ForEachEntity<MeshHandle, Transform, Optional<BoundingBox>>([&](const EntityID id, const MeshHandle *handle, const Transform *transform, const BoundingBox *bounds) {
    auto resolvedId = handle->assetId;
    if (!app.GetAsset<MeshAsset>(resolvedId))
      if (auto fallback = app.GetAsset("Cube"); fallback)
        resolvedId = fallback->id;
    if (auto it = assetToMesh.find(resolvedId); it != assetToMesh.end())
      Add(&it->second, scene.GetEntityComponent<DXMaterial>(id), transform, bounds, id);
  });
  scene.ForEachEntity<ModelMeshHandle, Transform, Optional<BoundingBox>>([&](const EntityID id, const ModelMeshHandle *handle, const Transform *transform, const BoundingBox *bounds) {
    auto modelIt = modelToMeshes.find(handle->modelAssetId);
    if (modelIt == modelToMeshes.end())
      return;
    auto meshIt = modelIt->second.find(handle->meshIndex);
    if (meshIt == modelIt->second.end())
      return;
    auto material = scene.GetEntityComponent<DXMaterial>(id);
    if (!material)
      if (const auto parent = scene.GetParent(id); parent)
        material = scene.GetEntityComponent<DXMaterial>(parent);
    Add(&meshIt->second, material, transform, bounds, id);
  });
  uint64_t required = instanceCursor;
  for (size_t index = 0; index < used; ++index) {
    auto &batch = batchPool[index];
    batch.visibleCount = static_cast<uint32_t>(batch.instances.size());
    // only the visible prefix is ever uploaded, so that is what the arena has to hold
    required += batch.instances.size() * sizeof(DXInstanceData) + D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT;
    batch.instances.insert(batch.instances.end(), hiddenPool[index].begin(), hiddenPool[index].end());
  }
  EnsureInstanceCapacity(required);
  return std::span(batchPool).first(used);
}
auto DXRenderer::BuildRayScene(const Camera &camera, std::span<const DXDrawBatch> batches) -> void {
  KUKI_PROFILE_SCOPE("BuildRayScene");
  auto context = GetContext();
  if (!context || !context->GetCapabilities().SupportsInlineRaytracing())
    return;
  std::vector<DXRayInstance> instances;
  for (const auto &batch : batches) {
    if (!batch.mesh || !*batch.mesh)
      continue;
    const auto textures = batch.material && batch.material->textureTableGPU ? batch.material->textureTableIndex : DXDescriptorHeap::InvalidIndex;
    const auto fallback = batch.material ? batch.material->fallback : MaterialFallback{};
    const auto unlit = batch.material && batch.material->type == MaterialType::Unlit;
    for (const auto &instance : batch.instances) {
      DXRayInstance placement{};
      placement.mesh = batch.mesh;
      memcpy(placement.transform, instance.model, sizeof(placement.transform));
      memcpy(placement.albedo, glm::value_ptr(fallback.albedo), sizeof(placement.albedo));
      memcpy(placement.emissive, glm::value_ptr(unlit ? fallback.albedo : fallback.emissive), sizeof(placement.emissive));
      memcpy(placement.attenuation, glm::value_ptr(fallback.attenuation), sizeof(placement.attenuation));
      placement.transmission = fallback.transmission;
      placement.thickness = fallback.thickness;
      placement.alphaCutoff = fallback.alphaCutoff;
      placement.alphaMode = static_cast<uint32_t>(fallback.alphaMode);
      placement.materialTextures = textures;
      placement.textureMask = static_cast<uint32_t>(fallback.textureMask.to_ulong());
      placement.entityId = instance.entityId;
      instances.push_back(placement);
    }
  }
  // what the probe trace sees of the scene, so it can tell whether its estimates are still of it.
  // the mesh is hashed by address and the rest as words, up to the last field rather than to the
  // end of the structure: the padding after it is never written and would hash differently every
  // frame, which would read as a scene that never holds still
  constexpr auto begin = offsetof(DXRayInstance, transform);
  constexpr auto end = offsetof(DXRayInstance, entityId) + sizeof(DXRayInstance::entityId);
  raySceneHash = instances.size();
  for (const auto &instance : instances) {
    hash_combine(raySceneHash, reinterpret_cast<uintptr_t>(instance.mesh));
    HashWords(raySceneHash, reinterpret_cast<const uint8_t *>(&instance) + begin, end - begin);
  }
  if (!rayScene.Build(*context, instances))
    return;
  const auto inverseViewProjection = glm::inverse(camera.transform.projection * camera.transform.view);
  rayScene.Validate(*context, pipelines, glm::value_ptr(inverseViewProjection), glm::value_ptr(camera.position));
}
auto DXRenderer::CollectProbeGeometry(Scene &scene) -> std::vector<DXProbeGeometry> {
  KUKI_PROFILE_SCOPE("CollectProbeGeometry");
  std::vector<DXProbeGeometry> geometry;
  const auto Add = [&](const Mesh &mesh, const Transform *transform) {
    if (mesh.vertices.empty())
      return;
    geometry.push_back({mesh.vertices, mesh.indices, transform->world});
  };
  scene.ForEachEntity<MeshHandle, Transform>([&](const EntityID, const MeshHandle *handle, const Transform *transform) {
    auto resolvedId = handle->assetId;
    if (!app.GetAsset<MeshAsset>(resolvedId))
      if (auto fallback = app.GetAsset("Cube"); fallback)
        resolvedId = fallback->id;
    if (auto meshAsset = app.GetAsset<MeshAsset>(resolvedId); meshAsset)
      Add(meshAsset->mesh, transform);
  });
  scene.ForEachEntity<ModelMeshHandle, Transform>([&](const EntityID, const ModelMeshHandle *handle, const Transform *transform) {
    auto modelAsset = app.GetAsset<ModelAsset>(handle->modelAssetId);
    if (!modelAsset || handle->meshIndex >= modelAsset->meshes.size())
      return;
    if (modelAsset->meshes[handle->meshIndex].bones.empty())
      Add(modelAsset->meshes[handle->meshIndex].mesh, transform);
  });
  return geometry;
}
auto DXRenderer::BuildProbeVolume(Scene &scene) -> void {
  KUKI_PROFILE_SCOPE("BuildProbeVolume");
  auto context = GetContext();
  if (!context)
    return;
  if (!probeVolume.Build(*context, CollectProbeGeometry(scene)))
    return;
  probeVolume.Validate(*context, pipelines);
}
auto DXRenderer::BindProbeVolume(const D3D12_GPU_VIRTUAL_ADDRESS fallback) -> void {
  auto context = GetContext();
  auto *commandList = context ? context->GetCommandList() : nullptr;
  if (!commandList)
    return;
  const auto ready = probeVolume.IsReady();
  commandList->SetGraphicsRootShaderResourceView(8, ready ? probeVolume.GetProbeAddress() : fallback);
  commandList->SetGraphicsRootShaderResourceView(9, ready ? probeVolume.GetNodeAddress() : fallback);
  commandList->SetGraphicsRootShaderResourceView(10, ready ? probeVolume.GetLookupAddress() : fallback);
}
auto DXRenderer::RetireDescriptorRange(const uint32_t first, const uint32_t count) -> void {
  if (first == DXDescriptorHeap::InvalidIndex || count == 0)
    return;
  retiredRanges.emplace_back(uploadEpoch, first, count);
}
auto DXRenderer::ReleaseMaterialTable(DXMaterial &material) -> void {
  if (!material.textureTableGPU)
    return;
  RetireDescriptorRange(material.textureTableIndex, MATERIAL_TEXTURE_SLOTS);
  material.textureTableIndex = DXDescriptorHeap::InvalidIndex;
  material.textureTableGPU = 0;
}
auto DXRenderer::ReleaseTexture(DXTexture &texture) -> void {
  if (auto context = GetContext(); context)
    context->GetSRVHeap().Free(texture.srvIndex);
  texture = {};
}
auto DXRenderer::RetireUploadBuffer(ComPtr<ID3D12Resource> buffer) -> void {
  if (buffer)
    retiredUploadBuffers.emplace_back(uploadEpoch, std::move(buffer));
}
auto DXRenderer::EnsureFrameConstantBuffer(const uint64_t slots) -> bool {
  if (frameConstantData && frameConstantSlots >= slots)
    return true;
  auto context = GetContext();
  if (!context || !context->GetDevice())
    return false;
  frameConstantStride = (sizeof(DXFrameConstants) + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) & ~static_cast<uint64_t>(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
  // Enough that a frame opening a folder full of assets does not grow it repeatedly, and small
  // enough not to matter: a slot is a few hundred bytes.
  auto capacity = frameConstantSlots > 0 ? frameConstantSlots : 64;
  while (capacity < slots)
    capacity *= 2;
  const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
  auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(frameConstantStride * capacity * DX_FRAME_COUNT);
  ComPtr<ID3D12Resource> grown;
  if (DXFailed(context->GetDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&grown)), "CreateCommittedResource for frame constants"))
    return false;
  void *mapped{};
  const CD3DX12_RANGE readRange(0, 0);
  if (DXFailed(grown->Map(0, &readRange, &mapped), "Map frame constants"))
    return false;
  // the outgoing buffer is retired rather than released: this can run mid-frame, and the command
  // list already holds addresses into it
  RetireUploadBuffer(std::move(frameConstantBuffer));
  frameConstantBuffer = std::move(grown);
  frameConstantData = static_cast<uint8_t *>(mapped);
  frameConstantSlots = capacity;
  return true;
}
auto DXRenderer::UploadFrameConstants(const DXFrameConstants &constants) -> D3D12_GPU_VIRTUAL_ADDRESS {
  auto context = GetContext();
  // A frame uploads these more than once. The scene pass does, and so does every preview drawn
  // after it, which happens while the same command list is still being recorded. Sharing one slot
  // meant the last writer decided what every earlier draw would read.
  if (!context || !EnsureFrameConstantBuffer(frameConstantCursor + 1))
    return 0;
  const auto frameOffset = frameConstantStride * frameConstantSlots * (context->GetFrameIndex() % DX_FRAME_COUNT);
  const auto offset = frameOffset + frameConstantStride * frameConstantCursor;
  ++frameConstantCursor;
  memcpy(frameConstantData + offset, &constants, sizeof(constants));
  return frameConstantBuffer->GetGPUVirtualAddress() + offset;
}
auto DXRenderer::BuildFrameConstants(Scene &scene, const Camera &camera, const bool hasShadowMap, DXFrameConstants &constants) const -> void {
  const auto viewProjection = camera.transform.projection * camera.transform.view;
  memcpy(constants.viewProjection, glm::value_ptr(viewProjection), sizeof(constants.viewProjection));
  memcpy(constants.viewPosition, glm::value_ptr(camera.position), sizeof(float) * 3);
  const Light *directional{};
  auto pointCount = 0u;
  auto spotCount = 0u;
  scene.ForEachEntity<Light>([&](const EntityID, const Light *light) {
    switch (light->type) {
    case LightType::Directional:
      directional = light;
      break;
    case LightType::Point:
      if (pointCount < MAX_POINT_LIGHTS) {
        auto &target = constants.pointLights[pointCount++];
        memcpy(target.position, glm::value_ptr(light->position), sizeof(float) * 3);
        memcpy(target.diffuse, glm::value_ptr(light->diffuse), sizeof(float) * 3);
        memcpy(target.specular, glm::value_ptr(light->specular), sizeof(float) * 3);
        target.attenuation[0] = light->constant;
        target.attenuation[1] = light->linear;
        target.attenuation[2] = light->quadratic;
        target.attenuation[3] = light->intensity;
      }
      break;
    case LightType::Spot:
      if (spotCount < MAX_SPOT_LIGHTS) {
        auto &target = constants.spotLights[spotCount++];
        memcpy(target.position, glm::value_ptr(light->position), sizeof(float) * 3);
        memcpy(target.direction, glm::value_ptr(light->forward), sizeof(float) * 3);
        memcpy(target.diffuse, glm::value_ptr(light->diffuse), sizeof(float) * 3);
        memcpy(target.specular, glm::value_ptr(light->specular), sizeof(float) * 3);
        target.attenuation[0] = light->constant;
        target.attenuation[1] = light->linear;
        target.attenuation[2] = light->quadratic;
        target.attenuation[3] = light->intensity;
        target.cutoff[0] = light->innerCutoff;
        target.cutoff[1] = light->outerCutoff;
      }
      break;
    }
  });
  if (directional) {
    memcpy(constants.directionalDirection, glm::value_ptr(directional->forward), sizeof(float) * 3);
    memcpy(constants.directionalAmbient, glm::value_ptr(directional->ambient), sizeof(float) * 3);
    memcpy(constants.directionalDiffuse, glm::value_ptr(directional->diffuse), sizeof(float) * 3);
    memcpy(constants.directionalSpecular, glm::value_ptr(directional->specular), sizeof(float) * 3);
    constants.directionalIntensity[0] = directional->intensity;
  }
  for (auto i = 0u; i < spotShadowCount && i < MAX_SPOT_LIGHTS; ++i)
    memcpy(constants.spotLightViewProjection[i], glm::value_ptr(spotShadowViewProjection[i]), sizeof(float) * 16);
  auto hasSkybox = false;
  scene.ForEachEntity<SkyboxHandle>([&](const EntityID, const SkyboxHandle *) {
    hasSkybox = true;
  });
  constants.counts[0] = pointCount;
  constants.counts[1] = spotCount;
  constants.counts[2] = directional ? 1u : 0u;
  constants.counts[3] = hasShadowMap ? 1u : 0u;
  constants.flags[0] = hasSkybox ? 1u : 0u;
  constants.flags[1] = spotShadowCount;
  constants.flags[2] = environmentReady ? 1u : 0u;
  // Set before the early return below, not after it. A volume that is not ready is exactly when the
  // views asking about the volume are worth having, and one of them says so in as many words -- the
  // fallback view is blue over every pixel when there is nothing to sample.
  constants.debug[0] = static_cast<uint32_t>(lightingDebugView);
  // Set before the early return for the same reason as the line above: a scene with no volume still
  // has a sky and a fallback ambient, and both are scaled from here.
  constants.indirectScale[0] = indirect.bounceIntensity;
  constants.indirectScale[1] = indirect.skyIntensity;
  constants.indirectScale[2] = indirect.ambientFallback;
  constants.probeTuning[0] = indirect.probeSurfaceBias;
  constants.probeTuning[1] = indirect.probeWeightFloor;
  constants.probeTuning[2] = indirect.probeVisibilitySharpness;
  if (!probeVolume.IsReady())
    return;
  probeVolume.GetBounds(constants.probeVolume, constants.probeVolume + 3);
  constants.probeCounts[0] = probeVolume.GetProbeCount();
  constants.probeCounts[1] = probeVolume.GetNodeCount();
  constants.probeCounts[2] = probeVolume.GetLookupResolution();
  constants.probeCounts[3] = 1u;
}
auto DXRenderer::FitSpotShadowFrustum(const Light &light, glm::mat4 &view, glm::mat4 &projection) const -> void {
  constexpr auto MAX_FOV = glm::radians(170.f);
  const auto fov = std::min(MAX_FOV, 2.f * std::acos(std::clamp(light.outerCutoff, -1.f, 1.f)) * 1.05f);
  view = glm::lookAtRH(light.position, light.position + light.forward, light.up);
  projection = glm::perspectiveRH_ZO(fov, 1.f, light.nearPlane, light.farPlane);
}
auto DXRenderer::DrawSkybox(const Camera &camera, const DXRenderTarget &target) -> void {
  // Not drawn under a debug view. The sky is not shaded by the scene pass and so is the one part of
  // the picture a view cannot replace, which would leave a photograph of a sunset filling the space
  // around a field of occlusion values -- the brightest thing on the screen, meaning nothing, and
  // read on the same scale as everything that does mean something. Black is the honest background:
  // there is no value here.
  if (lightingDebugView != LightingDebugView::None)
    return;
  auto context = GetContext();
  if (!context)
    return;
  auto *commandList = context->GetCommandList();
  auto scene = app.GetScene();
  if (!commandList || !scene || !EnsureComputeFallbacks())
    return;
  const auto samples = static_cast<uint32_t>(target.desc.samples > 0 ? target.desc.samples : 1);
  const auto pipeline = pipelines.GetSkyboxPipeline(context->GetDevice(), TargetFormatToRTVDXGI(target.desc.format), samples);
  if (!pipeline || !*pipeline)
    return;
  DXSkyboxConstants constants{};
  const auto rotation = glm::mat4(glm::mat3(camera.transform.view));
  const auto inverseViewProjection = glm::inverse(camera.transform.projection * rotation);
  memcpy(constants.inverseViewProjection, glm::value_ptr(inverseViewProjection), sizeof(constants.inverseViewProjection));
  const SkyboxHandle *skybox{};
  scene->ForEachEntity<SkyboxHandle>([&](const EntityID, const SkyboxHandle *handle) {
    if (!skybox)
      skybox = handle;
  });
  auto table = fallbackSkyboxTable;
  if (skybox) {
    constants.useGradient = 1u;
    if (environmentReady && environmentAsset == skybox->assetId && skyboxTableIndex != DXDescriptorHeap::InvalidIndex) {
      table = skyboxTable;
      constants.useTexture = 1u;
    }
  }
  commandList->SetGraphicsRootSignature(pipeline->rootSignature.Get());
  commandList->SetPipelineState(pipeline->pipelineState.Get());
  commandList->SetGraphicsRoot32BitConstants(0, sizeof(DXSkyboxConstants) / sizeof(uint32_t), &constants, 0);
  commandList->SetGraphicsRootDescriptorTable(1, table);
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commandList->IASetVertexBuffers(0, 0, nullptr);
  commandList->DrawInstanced(3, 1, 0, 0);
}
auto DXRenderer::BuildShadowMatrix(Scene &scene, float *out) const -> bool {
  KUKI_PROFILE_SCOPE("BuildShadowMatrix");
  if (!out)
    return false;
  const Light *directional{};
  scene.ForEachEntity<Light>([&](const EntityID, const Light *light) {
    if (light->type == LightType::Directional)
      directional = light;
  });
  if (!directional)
    return false;
  BoundingBox sceneBounds{};
  scene.ForEachEntity<BoundingBox, Transform>([&](const EntityID, const BoundingBox *box, const Transform *transform) {
    const auto worldBounds = box->GetWorldBounds(transform->world);
    sceneBounds.min = glm::min(sceneBounds.min, worldBounds.min);
    sceneBounds.max = glm::max(sceneBounds.max, worldBounds.max);
  });
  glm::mat4 view{1.f};
  glm::mat4 projection{1.f};
  if (!sceneBounds) {
    view = directional->GetView();
    projection = glm::orthoRH_ZO(-directional->orthoSize, directional->orthoSize, -directional->orthoSize, directional->orthoSize, directional->nearPlane, directional->farPlane);
  } else {
    const auto center = (sceneBounds.min + sceneBounds.max) * .5f;
    const auto radius = glm::length(sceneBounds.max - sceneBounds.min) * .5f;
    view = glm::lookAtRH(center - directional->forward * radius * 2.f, center, directional->up);
    auto boundsMin = glm::vec3(std::numeric_limits<float>::max());
    auto boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
    const glm::vec3 corners[8]{
      {sceneBounds.min.x, sceneBounds.min.y, sceneBounds.min.z},
      {sceneBounds.max.x, sceneBounds.min.y, sceneBounds.min.z},
      {sceneBounds.min.x, sceneBounds.max.y, sceneBounds.min.z},
      {sceneBounds.max.x, sceneBounds.max.y, sceneBounds.min.z},
      {sceneBounds.min.x, sceneBounds.min.y, sceneBounds.max.z},
      {sceneBounds.max.x, sceneBounds.min.y, sceneBounds.max.z},
      {sceneBounds.min.x, sceneBounds.max.y, sceneBounds.max.z},
      {sceneBounds.max.x, sceneBounds.max.y, sceneBounds.max.z}};
    for (const auto &corner : corners) {
      const auto viewSpace = glm::vec3(view * glm::vec4(corner, 1.f));
      boundsMin = glm::min(boundsMin, viewSpace);
      boundsMax = glm::max(boundsMax, viewSpace);
    }
    constexpr auto PADDING = .5f;
    const auto nearPlane = std::max(.01f, -boundsMax.z - PADDING);
    const auto farPlane = -boundsMin.z + PADDING;
    projection = glm::orthoRH_ZO(boundsMin.x - PADDING, boundsMax.x + PADDING, boundsMin.y - PADDING, boundsMax.y + PADDING, nearPlane, farPlane);
  }
  const auto matrix = projection * view;
  memcpy(out, glm::value_ptr(matrix), sizeof(float) * 16);
  return true;
}
auto DXRenderer::CreateTarget(const TargetDescription &desc, const std::string &name) -> EntityID {
  if (auto it = nameToId.find(name); it != nameToId.end()) {
    UpdateTarget(name, desc);
    return it->second;
  }
  DXRenderTarget target;
  if (!AllocateTarget(target, desc, name))
    return EntityID::Invalid;
  const auto id = nextTargetId;
  nextTargetId = static_cast<EntityID>(static_cast<uint32_t>(nextTargetId) + 1);
  nameToTarget.emplace(name, std::move(target));
  nameToId.emplace(name, id);
  return id;
}
auto DXRenderer::UpdateTarget(const std::string &name, const TargetDescription &desc) -> void {
  auto it = nameToTarget.find(name);
  if (it == nameToTarget.end())
    return;
  if (it->second.desc == desc)
    return;
  if (auto context = GetContext(); context) {
    if (context->GetCommandList())
      context->FlushCommandList();
    else
      context->WaitForGPU();
  }
  AllocateTarget(it->second, desc, name);
}
auto DXRenderer::GetTarget(const std::string &name) -> RenderTarget * {
  auto it = nameToTarget.find(name);
  if (it == nameToTarget.end())
    return nullptr;
  auto &target = it->second;
  if (target.resource)
    Transition(target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  return &target;
}
auto DXRenderer::GetCapabilities() const -> RendererCapabilities {
  const auto *context = GetContext();
  const auto traces = context && context->GetCapabilities().SupportsBindless() && context->GetCapabilities().SupportsInlineRaytracing();
  // The debug views are not conditional on the same thing. They are branches in `scene.hlsl`, which
  // compiles and runs whatever the hardware can trace, and a field that never filled is worth
  // looking at through them more than a full one is.
  return {.probeVolume = traces, .lightingDebugViews = true};
}
auto DXRenderer::TraceProbes(std::span<std::string>, std::span<std::string>) -> void {
  auto context = GetContext();
  if (!context)
    return;
  auto scene = app.GetScene();
  if (!scene)
    return;
  auto camera = scene->GetCamera();
  if (!camera)
    return;
  // Collected here rather than handed across from the scene pass, which collects its own. Two walks
  // of the scene is what the passes not knowing about each other costs, and beside a dispatch of
  // sixty-four rays for every probe it is not worth paying for by making the scene pass unrunnable
  // unless this one ran first -- which is the arrangement moving this out was meant to end.
  //
  // Frustum culling does not divide the two. `CollectBatches` puts the hidden instances back after
  // recording where the visible ones ended, so what comes out is the whole scene either way, which
  // is what a probe standing behind the camera needs the acceleration structure to contain.
  const auto batches = CollectBatches(*scene, false, camera);
  BuildRayScene(*camera, batches);
  BuildProbeVolume(*scene);
  DXFrameConstants frameConstants{};
  // No shadow map, and nothing lost by it. The trace asks the acceleration structure whether a light
  // is reachable rather than sampling a map, so it has no such input to declare and nothing to wait
  // for. The flag reaches the hash below, so leaving it false also stops a shadow map arriving or
  // going from winding the running mean back over a change the trace cannot see. A light itself
  // arriving still winds it back: the direction, the colour and the count are all in the same range.
  BuildFrameConstants(*scene, *camera, false, frameConstants);
  const auto frameConstantAddress = UploadFrameConstants(frameConstants);
  if (!frameConstantAddress)
    return;
  auto traceHash = raySceneHash;
  hash_combine(traceHash, HashLighting(frameConstants));
  hash_combine(traceHash, HashProbeTuning(frameConstants));
  // The trace's own settings are hashed by the volume rather than here, since it is the one that
  // knows which of them it actually put into the dispatch.
  probeVolume.Trace(*context, pipelines, rayScene, frameConstantAddress, shBuffer ? shBuffer->GetGPUVirtualAddress() : 0, traceHash, indirect);
  probeVolume.Report(*context);
}
auto DXRenderer::RenderScene(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  ClearOutputs(outputs);
  auto context = GetContext();
  if (!context || outputs.empty())
    return;
  auto *commandList = context->GetCommandList();
  auto targetIt = nameToTarget.find(outputs[0]);
  if (!commandList || targetIt == nameToTarget.end() || !targetIt->second.resource)
    return;
  auto &target = targetIt->second;
  auto scene = app.GetScene();
  if (!scene)
    return;
  auto camera = scene->GetCamera();
  if (!camera)
    return;
  if (!EnsureDepthBuffer(target))
    return;
  EnsureSceneResources(*scene);
  const auto batches = CollectBatches(*scene, false, camera);
  const auto samples = static_cast<uint32_t>(target.desc.samples > 0 ? target.desc.samples : 1);
  const auto pipeline = pipelines.GetScenePipeline(context->GetDevice(), TargetFormatToRTVDXGI(target.desc.format), samples);
  if (!pipeline || !*pipeline)
    return;
  auto fallbackTexture = EnsureDummyTexture();
  if (!fallbackTexture)
    return;
  DXFrameConstants frameConstants{};
  auto shadowSrv = fallbackTexture->srvGPU;
  auto spotShadowSrv = dummyArraySrvGPU.ptr ? dummyArraySrvGPU : fallbackTexture->srvGPU;
  auto hasShadowMap = false;
  for (const auto &name : inputs) {
    auto inputIt = nameToTarget.find(name);
    if (inputIt == nameToTarget.end() || !inputIt->second.resource || !IsDepthFormat(inputIt->second.desc.format) || inputIt->second.srvIndex == DXDescriptorHeap::InvalidIndex)
      continue;
    if (inputIt->second.desc.type == TargetType::Texture2DArray) {
      Transition(inputIt->second, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      spotShadowSrv = inputIt->second.srvGPU;
      continue;
    }
    if (hasShadowMap)
      continue;
    if (float matrix[16]{}; BuildShadowMatrix(*scene, matrix)) {
      Transition(inputIt->second, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      memcpy(frameConstants.lightViewProjection, matrix, sizeof(matrix));
      shadowSrv = inputIt->second.srvGPU;
      hasShadowMap = true;
    }
  }
  // The field this shades out of was filled by the `ProbeTrace` pass, which the graph puts ahead of
  // this one because this one names `ProbeField` among its inputs. That ordering is also what makes
  // the probe bounds gathered below current: the volume is rebuilt there, before they are read.
  BuildFrameConstants(*scene, *camera, hasShadowMap, frameConstants);
  const auto frameConstantAddress = UploadFrameConstants(frameConstants);
  if (!frameConstantAddress)
    return;
  Transition(target, D3D12_RESOURCE_STATE_RENDER_TARGET);
  const auto dsv = context->GetDSVHeap().GetCPUHandle(target.depthDsvIndex);
  const auto hasIdBuffer = target.idResource && target.idRtvIndex != DXDescriptorHeap::InvalidIndex;
  D3D12_CPU_DESCRIPTOR_HANDLE rtvs[2]{context->GetRTVHeap().GetCPUHandle(target.rtvIndex)};
  if (hasIdBuffer) {
    rtvs[1] = context->GetRTVHeap().GetCPUHandle(target.idRtvIndex);
    constexpr float ID_CLEAR[4]{1.f, 1.f, 1.f, 1.f};
    commandList->ClearRenderTargetView(rtvs[1], ID_CLEAR, 0, nullptr);
  }
  commandList->OMSetRenderTargets(hasIdBuffer ? 2 : 1, rtvs, FALSE, &dsv);
  commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
  DrawSkybox(*camera, target);
  commandList->SetGraphicsRootSignature(pipeline->rootSignature.Get());
  commandList->SetPipelineState(pipeline->pipelineState.Get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commandList->SetGraphicsRootConstantBufferView(1, frameConstantAddress);
  commandList->SetGraphicsRootDescriptorTable(3, shadowSrv);
  commandList->SetGraphicsRootDescriptorTable(4, spotShadowSrv);
  commandList->SetGraphicsRootDescriptorTable(7, environmentReady ? environmentTable : fallbackEnvironmentTable);
  commandList->SetGraphicsRootDescriptorTable(11, fallbackTexture->srvGPU);
  BindProbeVolume(frameConstantAddress);
  auto drawnInstances = 0;
  const auto ApplyMaterial = [&](const DXMaterial *material) {
    DXSceneConstants constants{};
    const auto fallback = material ? material->fallback : MaterialFallback{};
    memcpy(constants.albedo, glm::value_ptr(fallback.albedo), sizeof(constants.albedo));
    memcpy(constants.specular, glm::value_ptr(fallback.specular), sizeof(constants.specular));
    memcpy(constants.emissive, glm::value_ptr(fallback.emissive), sizeof(constants.emissive));
    constants.surface[0] = fallback.metalness;
    constants.surface[1] = fallback.occlusion;
    constants.surface[2] = fallback.roughness;
    memcpy(constants.attenuation, glm::value_ptr(fallback.attenuation), sizeof(constants.attenuation));
    constants.volume[0] = fallback.transmission;
    constants.volume[1] = fallback.thickness;
    constants.volume[2] = fallback.ior;
    const auto hasTable = material && material->textureTableGPU;
    constants.textureMask = hasTable ? static_cast<uint32_t>(fallback.textureMask.to_ulong()) : 0u;
    constants.unlit = material && material->type == MaterialType::Unlit ? 1u : 0u;
    constants.alphaCutoff = fallback.alphaCutoff;
    constants.alphaMode = static_cast<uint32_t>(fallback.alphaMode);
    commandList->SetGraphicsRoot32BitConstants(0, sizeof(DXSceneConstants) / sizeof(uint32_t), &constants, 0);
    D3D12_GPU_DESCRIPTOR_HANDLE materialTable{hasTable ? material->textureTableGPU : fallbackMaterialTable.ptr};
    commandList->SetGraphicsRootDescriptorTable(2, materialTable);
  };
  const auto DrawMesh = [&](const DXMesh *mesh, const UINT instances) {
    commandList->IASetVertexBuffers(0, 1, &mesh->vertexBufferView);
    if (mesh->indexCount > 0) {
      commandList->IASetIndexBuffer(&mesh->indexBufferView);
      commandList->DrawIndexedInstanced(mesh->indexCount, instances, 0, 0, 0);
    } else
      commandList->DrawInstanced(mesh->vertexCount, instances, 0, 0);
  };
  const auto Blended = [](const DXMaterial *material) {
    return material && material->fallback.alphaMode == AlphaMode::Blend;
  };
  const auto Transmissive = [](const DXMaterial *material) {
    return material && material->fallback.transmission > .0f;
  };
  const auto Distance = [&](const DXInstanceData &instance) {
    const glm::vec3 position{instance.model[12], instance.model[13], instance.model[14]};
    return glm::distance(position, camera->position);
  };
  std::vector<DXDeferredDraw> deferred;
  for (const auto &batch : batches) {
    // a batch may still hold instances the acceleration structure wants and the rasteriser does not
    if (batch.visibleCount == 0)
      continue;
    const auto address = AllocateInstances(std::span(batch.instances).first(batch.visibleCount));
    if (!address)
      continue;
    if (Blended(batch.material) || Transmissive(batch.material)) {
      deferred.push_back({batch.mesh, batch.material, address, address, batch.visibleCount, false, Blended(batch.material), Distance(batch.instances.front())});
      continue;
    }
    ApplyMaterial(batch.material);
    commandList->SetGraphicsRootShaderResourceView(5, address);
    commandList->SetGraphicsRootShaderResourceView(6, address);
    DrawMesh(batch.mesh, static_cast<UINT>(batch.visibleCount));
    drawnInstances += static_cast<int>(batch.visibleCount);
  }
  const auto skinnedDraws = CollectSkinnedDraws(*scene, camera);
  if (!skinnedDraws.empty()) {
    auto required = instanceCursor;
    for (const auto &draw : skinnedDraws)
      required += sizeof(DXInstanceData) + draw.bones.size() * sizeof(glm::mat4) + 2 * D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT;
    EnsureInstanceCapacity(required);
    if (const auto skinnedPipeline = pipelines.GetScenePipeline(context->GetDevice(), TargetFormatToRTVDXGI(target.desc.format), samples, true); skinnedPipeline && *skinnedPipeline) {
      commandList->SetPipelineState(skinnedPipeline->pipelineState.Get());
      for (const auto &draw : skinnedDraws) {
        const auto address = AllocateInstances(std::span(&draw.instance, 1));
        const auto boneAddress = draw.bones.empty() ? address : AllocateBones(draw.bones);
        if (!address || !boneAddress)
          continue;
        if (Blended(draw.material) || Transmissive(draw.material)) {
          deferred.push_back({draw.mesh, draw.material, address, boneAddress, 1u, true, Blended(draw.material), Distance(draw.instance)});
          continue;
        }
        ApplyMaterial(draw.material);
        commandList->SetGraphicsRootShaderResourceView(5, address);
        commandList->SetGraphicsRootShaderResourceView(6, boneAddress);
        DrawMesh(draw.mesh, 1);
        ++drawnInstances;
      }
    }
  }
  if (!deferred.empty()) {
    std::sort(deferred.begin(), deferred.end(), [](const DXDeferredDraw &first, const DXDeferredDraw &second) { return first.depth > second.depth; });
    const auto refracts = std::any_of(deferred.begin(), deferred.end(), [&](const DXDeferredDraw &draw) { return Transmissive(draw.material); });
    if (refracts && CaptureSceneColor(target))
      commandList->SetGraphicsRootDescriptorTable(11, sceneColorTable);
    const DXPipeline *current{};
    for (const auto &draw : deferred) {
      const auto deferredPipeline = pipelines.GetScenePipeline(context->GetDevice(), TargetFormatToRTVDXGI(target.desc.format), samples, draw.skinned, draw.blended);
      if (!deferredPipeline || !*deferredPipeline)
        continue;
      if (deferredPipeline != current) {
        commandList->SetPipelineState(deferredPipeline->pipelineState.Get());
        current = deferredPipeline;
      }
      ApplyMaterial(draw.material);
      commandList->SetGraphicsRootShaderResourceView(5, draw.instances);
      commandList->SetGraphicsRootShaderResourceView(6, draw.bones);
      DrawMesh(draw.mesh, draw.count);
      drawnInstances += static_cast<int>(draw.count);
    }
  }
  DrawProbes(target, frameConstants.viewProjection);
      static auto lastBatches = -1;
  static auto lastInstances = -1;
  if (static_cast<int>(batches.size()) != lastBatches || drawnInstances != lastInstances) {
    lastBatches = static_cast<int>(batches.size());
    lastInstances = drawnInstances;
    spdlog::info("[DXRenderer] scene pass drew {} instances in {} batches plus {} skinned meshes", drawnInstances, batches.size(), skinnedDraws.size());
  }
}
auto DXRenderer::EnsureSceneColor(const DXRenderTarget &target) -> bool {
  auto context = GetContext();
  if (!context || !context->GetDevice() || !target.resource)
    return false;
  const auto format = TargetFormatToSRVDXGI(target.desc.format);
  if (sceneColorCopy && sceneColorWidth == target.desc.width && sceneColorHeight == target.desc.height && sceneColorFormat == format)
    return true;
  auto *device = context->GetDevice();
  if (sceneColorCopy)
    context->WaitForGPU();
  sceneColorCopy.Reset();
  auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, static_cast<UINT64>(target.desc.width), static_cast<UINT>(target.desc.height), 1, 1);
  const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  if (DXFailed(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&sceneColorCopy)), "CreateCommittedResource for SceneColor"))
    return false;
  sceneColorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  sceneColorWidth = target.desc.width;
  sceneColorHeight = target.desc.height;
  sceneColorFormat = format;
  auto &srvHeap = context->GetSRVHeap();
  if (sceneColorSrvIndex == DXDescriptorHeap::InvalidIndex) {
    sceneColorSrvIndex = srvHeap.Allocate();
    if (sceneColorSrvIndex == DXDescriptorHeap::InvalidIndex)
      return false;
    sceneColorTable = srvHeap.GetGPUHandle(sceneColorSrvIndex);
  }
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = format;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels = 1;
  device->CreateShaderResourceView(sceneColorCopy.Get(), &srvDesc, srvHeap.GetCPUHandle(sceneColorSrvIndex));
  spdlog::info("[DXRenderer] created scene colour copy ({}x{})", sceneColorWidth, sceneColorHeight);
  return true;
}
auto DXRenderer::CaptureSceneColor(DXRenderTarget &target) -> bool {
  auto context = GetContext();
  if (!context || !EnsureSceneColor(target))
    return false;
  auto *commandList = context->GetCommandList();
  if (!commandList)
    return false;
  const auto multisampled = target.desc.samples > 1;
  const auto destination = multisampled ? D3D12_RESOURCE_STATE_RESOLVE_DEST : D3D12_RESOURCE_STATE_COPY_DEST;
  Transition(target, multisampled ? D3D12_RESOURCE_STATE_RESOLVE_SOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE);
  auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(sceneColorCopy.Get(), sceneColorState, destination);
  commandList->ResourceBarrier(1, &barrier);
  if (multisampled)
    commandList->ResolveSubresource(sceneColorCopy.Get(), 0, target.resource.Get(), 0, sceneColorFormat);
  else
    commandList->CopyResource(sceneColorCopy.Get(), target.resource.Get());
  barrier = CD3DX12_RESOURCE_BARRIER::Transition(sceneColorCopy.Get(), destination, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  commandList->ResourceBarrier(1, &barrier);
  sceneColorState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  Transition(target, D3D12_RESOURCE_STATE_RENDER_TARGET);
  return true;
}
auto DXRenderer::EnsureProbeMesh() -> DXMesh * {
  if (probeMesh)
    return &probeMesh;
  Mesh source;
  source.vertices = Primitive::Sphere(PROBE_DEBUG_SPHERE_LEVEL);
  probeMesh = UploadMeshData(source, "ProbeSphere");
  return probeMesh ? &probeMesh : nullptr;
}
auto DXRenderer::DrawProbes(const DXRenderTarget &target, const float *viewProjection) -> void {
  if (probeDebugView == ProbeDebugView::Off || !probeVolume.IsReady())
    return;
  auto context = GetContext();
  auto *commandList = context ? context->GetCommandList() : nullptr;
  if (!commandList)
    return;
  const auto sphere = EnsureProbeMesh();
  if (!sphere)
    return;
  const auto samples = static_cast<uint32_t>(target.desc.samples > 0 ? target.desc.samples : 1);
  const auto pipeline = pipelines.GetProbeDebugPipeline(context->GetDevice(), TargetFormatToRTVDXGI(target.desc.format), samples);
  if (!pipeline || !*pipeline)
    return;
  float origin[3]{};
  auto side = 0.f;
  probeVolume.GetBounds(origin, &side);
  DXProbeDebugConstants constants{};
  memcpy(constants.viewProjection, viewProjection, sizeof(constants.viewProjection));
  constants.scale = side / static_cast<float>(probeVolume.GetLookupResolution()) * PROBE_DEBUG_SPHERE_SCALE;
  constants.mode = static_cast<uint32_t>(probeDebugView);
  // The clamp the trace records distances against, in world units. Dividing by it is what makes a
  // white sphere mean "as far as this probe can see" rather than "far, in units you cannot read".
  constants.range = PROBE_DEPTH_RANGE * side;
  commandList->SetGraphicsRootSignature(pipeline->rootSignature.Get());
  commandList->SetPipelineState(pipeline->pipelineState.Get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commandList->SetGraphicsRoot32BitConstants(0, sizeof(DXProbeDebugConstants) / sizeof(uint32_t), &constants, 0);
  commandList->SetGraphicsRootShaderResourceView(1, probeVolume.GetProbeAddress());
  commandList->IASetVertexBuffers(0, 1, &sphere->vertexBufferView);
  if (sphere->indexCount > 0) {
    commandList->IASetIndexBuffer(&sphere->indexBufferView);
    commandList->DrawIndexedInstanced(sphere->indexCount, probeVolume.GetProbeCount(), 0, 0, 0);
  } else
    commandList->DrawInstanced(sphere->vertexCount, probeVolume.GetProbeCount(), 0, 0);
}
/// @brief Depth alone, through the camera, before anything is shaded.
///
/// The same geometry and the same pipeline a shadow map uses -- they differ only in whose eye the
/// scene is seen from, so the shadow pipeline serves both and the camera's matrix goes where the
/// light's would.
auto DXRenderer::CreateDepthPrepass(std::span<std::string>, std::span<std::string> outputs) -> void {
  ClearOutputs(outputs);
  auto context = GetContext();
  if (!context || outputs.empty())
    return;
  auto *commandList = context->GetCommandList();
  auto targetIt = nameToTarget.find(outputs[0]);
  if (!commandList || targetIt == nameToTarget.end() || !targetIt->second.resource)
    return;
  auto &target = targetIt->second;
  if (target.dsvIndex == DXDescriptorHeap::InvalidIndex)
    return;
  auto scene = app.GetScene();
  if (!scene)
    return;
  auto camera = scene->GetCamera();
  if (!camera)
    return;
  const auto pipeline = pipelines.GetShadowPipeline(context->GetDevice());
  if (!pipeline || !*pipeline)
    return;
  DXShadowConstants constants{};
  const auto viewProjection = camera->transform.projection * camera->transform.view;
  memcpy(constants.lightViewProjection, glm::value_ptr(viewProjection), sizeof(constants.lightViewProjection));
  Transition(target, D3D12_RESOURCE_STATE_DEPTH_WRITE);
  const auto dsv = context->GetDSVHeap().GetCPUHandle(target.dsvIndex);
  commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
  commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
  const auto viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<float>(target.desc.width), static_cast<float>(target.desc.height));
  const auto scissor = CD3DX12_RECT(0, 0, static_cast<LONG>(target.desc.width), static_cast<LONG>(target.desc.height));
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  commandList->SetGraphicsRootSignature(pipeline->rootSignature.Get());
  commandList->SetPipelineState(pipeline->pipelineState.Get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commandList->SetGraphicsRoot32BitConstants(0, sizeof(DXShadowConstants) / sizeof(uint32_t), &constants, 0);
  for (const auto &batch : CollectBatches(*scene, true)) {
    const auto address = AllocateInstances(batch.instances);
    if (!address)
      continue;
    commandList->SetGraphicsRootShaderResourceView(1, address);
    commandList->IASetVertexBuffers(0, 1, &batch.mesh->vertexBufferView);
    const auto instances = static_cast<UINT>(batch.visibleCount);
    if (batch.mesh->indexCount > 0) {
      commandList->IASetIndexBuffer(&batch.mesh->indexBufferView);
      commandList->DrawIndexedInstanced(batch.mesh->indexCount, instances, 0, 0, 0);
    } else
      commandList->DrawInstanced(batch.mesh->vertexCount, instances, 0, 0);
  }
}
auto DXRenderer::ApplyAntiAliasing(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  BlitOrResolve(inputs, outputs);
}
auto DXRenderer::ApplyBloomEffect(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  ApplyFullscreenEffect(inputs, outputs, "PSBloom", BLOOM_INTENSITY);
}
auto DXRenderer::ApplyBlurEffect(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  if (inputs.size() != 1 || outputs.empty()) {
    BlitOrResolve(inputs, outputs);
    return;
  }
  const auto ping = outputs[0] + "Ping";
  const auto pong = outputs[0] + "Pong";
  if (!nameToTarget.contains(ping) || !nameToTarget.contains(pong)) {
    BlitOrResolve(inputs, outputs);
    return;
  }
  for (uint32_t pass = 0; pass < BLUR_PASS_COUNT; ++pass) {
    const auto horizontal = pass % 2 == 0;
    std::array<std::string, 1> passInput{pass == 0 ? inputs[0] : horizontal ? pong
                                                                            : ping};
    std::array<std::string, 1> passOutput{pass == BLUR_PASS_COUNT - 1 ? outputs[0] : horizontal ? ping
                                                                                                : pong};
    ApplyFullscreenEffect(passInput, passOutput, "PSBlur", 0.f, horizontal);
  }
}
auto DXRenderer::ApplyBrightPassFilter(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  ApplyFullscreenEffect(inputs, outputs, "PSBrightPass", BRIGHT_PASS_THRESHOLD);
}
auto DXRenderer::ApplyToneMapping(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  ApplyFullscreenEffect(inputs, outputs, "PSToneMapping", GAMMA);
}
auto DXRenderer::ApplyOutline(std::span<std::string> inputs, std::span<std::string> outputs) -> void {
  std::array<std::string, 1> colorInput{};
  for (const auto &name : inputs)
    if (auto it = nameToTarget.find(name); it != nameToTarget.end() && it->second.resource && it->second.desc.samples <= 1) {
      colorInput[0] = name;
      break;
    }
  const auto PassThrough = [&] {
    if (colorInput[0].empty())
      BlitOrResolve(inputs, outputs);
    else
      BlitOrResolve(colorInput, outputs);
  };
  auto context = GetContext();
  if (!context || outputs.empty()) {
    PassThrough();
    return;
  }
  auto *commandList = context->GetCommandList();
  auto destinationIt = nameToTarget.find(outputs[0]);
  if (!commandList || destinationIt == nameToTarget.end() || !destinationIt->second.resource) {
    PassThrough();
    return;
  }
  DXRenderTarget *colorSource{};
  DXRenderTarget *idSource{};
  for (const auto &name : inputs) {
    auto it = nameToTarget.find(name);
    if (it == nameToTarget.end() || !it->second.resource)
      continue;
    if (!idSource && it->second.idResource && it->second.idSrvGPU.ptr)
      idSource = &it->second;
    if (!colorSource && it->second.desc.samples <= 1 && it->second.srvIndex != DXDescriptorHeap::InvalidIndex)
      colorSource = &it->second;
  }
  auto &destination = destinationIt->second;
  const auto selected = app.GetSelectedEntities();
  if (!colorSource || !idSource || selected.empty()) {
    PassThrough();
    return;
  }
  const auto pipeline = pipelines.GetOutlinePipeline(context->GetDevice(), TargetFormatToRTVDXGI(destination.desc.format));
  if (!pipeline || !*pipeline) {
    PassThrough();
    return;
  }
  Transition(*colorSource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  Transition(destination, D3D12_RESOURCE_STATE_RENDER_TARGET);
  const auto idToShaderResource = CD3DX12_RESOURCE_BARRIER::Transition(idSource->idResource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  commandList->ResourceBarrier(1, &idToShaderResource);
  const auto rtv = context->GetRTVHeap().GetCPUHandle(destination.rtvIndex);
  commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
  const auto viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<float>(destination.desc.width), static_cast<float>(destination.desc.height));
  const auto scissor = CD3DX12_RECT(0, 0, static_cast<LONG>(destination.desc.width), static_cast<LONG>(destination.desc.height));
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  commandList->SetGraphicsRootSignature(pipeline->rootSignature.Get());
  commandList->SetPipelineState(pipeline->pipelineState.Get());
  DXOutlineConstants constants{};
  constants.color[0] = 1.f;
  constants.color[1] = .6f;
  constants.color[2] = 0.f;
  constants.color[3] = 2.f;
  constants.texelSize[0] = destination.desc.width > 0 ? 1.f / destination.desc.width : 0.f;
  constants.texelSize[1] = destination.desc.height > 0 ? 1.f / destination.desc.height : 0.f;
  const auto count = static_cast<uint32_t>(std::min<size_t>(selected.size(), MAX_OUTLINE_SELECTED));
  constants.selectedCount[0] = count;
  for (uint32_t i = 0; i < count; ++i)
    constants.selected[i] = static_cast<uint32_t>(static_cast<long long>(selected[i])) & 0xFFFFFFu;
  commandList->SetGraphicsRoot32BitConstants(0, sizeof(DXOutlineConstants) / sizeof(uint32_t), &constants, 0);
  commandList->SetGraphicsRootDescriptorTable(1, colorSource->srvGPU);
  commandList->SetGraphicsRootDescriptorTable(2, idSource->idSrvGPU);
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commandList->IASetVertexBuffers(0, 0, nullptr);
  commandList->DrawInstanced(3, 1, 0, 0);
  const auto idBack = CD3DX12_RESOURCE_BARRIER::Transition(idSource->idResource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
  commandList->ResourceBarrier(1, &idBack);
}
auto DXRenderer::CreateShadowMap(std::span<std::string>, std::span<std::string> outputs) -> void {
  ClearOutputs(outputs);
  auto context = GetContext();
  if (!context || outputs.empty())
    return;
  auto *commandList = context->GetCommandList();
  auto targetIt = nameToTarget.find(outputs[0]);
  if (!commandList || targetIt == nameToTarget.end() || !targetIt->second.resource)
    return;
  auto &target = targetIt->second;
  if (target.dsvIndex == DXDescriptorHeap::InvalidIndex)
    return;
  auto scene = app.GetScene();
  if (!scene)
    return;
  DXShadowConstants constants{};
  if (!BuildShadowMatrix(*scene, constants.lightViewProjection))
    return;
  const auto pipeline = pipelines.GetShadowPipeline(context->GetDevice());
  if (!pipeline || !*pipeline)
    return;
  Transition(target, D3D12_RESOURCE_STATE_DEPTH_WRITE);
  const auto dsv = context->GetDSVHeap().GetCPUHandle(target.dsvIndex);
  commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
  commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
  const auto viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<float>(target.desc.width), static_cast<float>(target.desc.height));
  const auto scissor = CD3DX12_RECT(0, 0, static_cast<LONG>(target.desc.width), static_cast<LONG>(target.desc.height));
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  commandList->SetGraphicsRootSignature(pipeline->rootSignature.Get());
  commandList->SetPipelineState(pipeline->pipelineState.Get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commandList->SetGraphicsRoot32BitConstants(0, sizeof(DXShadowConstants) / sizeof(uint32_t), &constants, 0);
  for (const auto &batch : CollectBatches(*scene, true)) {
    const auto address = AllocateInstances(batch.instances);
    if (!address)
      continue;
    commandList->SetGraphicsRootShaderResourceView(1, address);
    commandList->IASetVertexBuffers(0, 1, &batch.mesh->vertexBufferView);
    const auto instances = static_cast<UINT>(batch.visibleCount);
    if (batch.mesh->indexCount > 0) {
      commandList->IASetIndexBuffer(&batch.mesh->indexBufferView);
      commandList->DrawIndexedInstanced(batch.mesh->indexCount, instances, 0, 0, 0);
    } else
      commandList->DrawInstanced(batch.mesh->vertexCount, instances, 0, 0);
  }
}
auto DXRenderer::CreateSpotShadowMap(std::span<std::string>, std::span<std::string> outputs) -> void {
  ClearOutputs(outputs);
  spotShadowCount = 0;
  auto context = GetContext();
  if (!context || outputs.empty())
    return;
  auto *commandList = context->GetCommandList();
  auto targetIt = nameToTarget.find(outputs[0]);
  if (!commandList || targetIt == nameToTarget.end() || !targetIt->second.resource)
    return;
  auto &target = targetIt->second;
  if (target.layerDsvIndices.empty())
    return;
  auto scene = app.GetScene();
  if (!scene)
    return;
  std::vector<const Light *> spotLights;
  scene->ForEachEntity<Light>([&](const EntityID, const Light *light) {
    if (light->type == LightType::Spot && spotLights.size() < MAX_SPOT_LIGHTS)
      spotLights.push_back(light);
  });
  if (spotLights.empty())
    return;
  const auto pipeline = pipelines.GetShadowPipeline(context->GetDevice());
  if (!pipeline || !*pipeline)
    return;
  Transition(target, D3D12_RESOURCE_STATE_DEPTH_WRITE);
  const auto viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<float>(target.desc.width), static_cast<float>(target.desc.height));
  const auto scissor = CD3DX12_RECT(0, 0, static_cast<LONG>(target.desc.width), static_cast<LONG>(target.desc.height));
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  commandList->SetGraphicsRootSignature(pipeline->rootSignature.Get());
  commandList->SetPipelineState(pipeline->pipelineState.Get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  const auto layers = std::min(spotLights.size(), target.layerDsvIndices.size());
  const auto batches = CollectBatches(*scene, true);
  std::vector<D3D12_GPU_VIRTUAL_ADDRESS> addresses;
  addresses.reserve(batches.size());
  for (const auto &batch : batches)
    addresses.push_back(AllocateInstances(batch.instances));
  for (size_t layer = 0; layer < layers; ++layer) {
    glm::mat4 view{1.f};
    glm::mat4 projection{1.f};
    FitSpotShadowFrustum(*spotLights[layer], view, projection);
    spotShadowViewProjection[layer] = projection * view;
    DXShadowConstants constants{};
    memcpy(constants.lightViewProjection, glm::value_ptr(spotShadowViewProjection[layer]), sizeof(constants.lightViewProjection));
    const auto dsv = context->GetDSVHeap().GetCPUHandle(target.layerDsvIndices[layer]);
    commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
    commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
    commandList->SetGraphicsRoot32BitConstants(0, sizeof(DXShadowConstants) / sizeof(uint32_t), &constants, 0);
    for (size_t index = 0; index < batches.size(); ++index) {
      if (!addresses[index])
        continue;
      commandList->SetGraphicsRootShaderResourceView(1, addresses[index]);
      commandList->IASetVertexBuffers(0, 1, &batches[index].mesh->vertexBufferView);
      const auto instances = static_cast<UINT>(batches[index].visibleCount);
      if (batches[index].mesh->indexCount > 0) {
        commandList->IASetIndexBuffer(&batches[index].mesh->indexBufferView);
        commandList->DrawIndexedInstanced(batches[index].mesh->indexCount, instances, 0, 0, 0);
      } else
        commandList->DrawInstanced(batches[index].mesh->vertexCount, instances, 0, 0);
    }
  }
  spotShadowCount = static_cast<uint32_t>(layers);
}
auto DXRenderer::Clear() -> void {
  probeVolume.Clear();
  rayScene.Clear();
  ReleaseAllTargets();
  pipelines.Clear();
  pickResolveTexture.Reset();
  pickReadbackBuffer.Reset();
  pickResolveWidth = 0;
  pickResolveHeight = 0;
  // Everything from here down runs with the GPU idle: `ReleaseAllTargets` above flushes the command
  // list and waits. That is what lets these descriptors go straight back to the heap instead of on
  // to the retirement queue, which is drained by `Reset` -- a call a backend being torn down is
  // never going to reach again.
  auto context = GetContext();
  // Guarded, because a backend can be torn down after its device has already gone -- and the rest
  // of this function, which drops the maps and the handles, still has to run when it has.
  const auto FreeSlot = [&](uint32_t &index) {
    if (context)
      context->GetSRVHeap().Free(index);
    index = DXDescriptorHeap::InvalidIndex;
  };
  const auto FreeSlots = [&](uint32_t &first, const uint32_t count) {
    if (context)
      context->GetSRVHeap().FreeRange(first, count);
    first = DXDescriptorHeap::InvalidIndex;
  };
  ReleaseTexture(dummyTexture);
  FreeSlot(dummyArraySrvIndex);
  dummyArraySrvGPU = {};
  // Every texture the session uploaded, each holding a view into the shared heap. The maps were
  // cleared without them, which let a backend that was torn down and stood up again in one session
  // start from a heap already spoken for by textures nothing could name any more.
  for (auto &[assetId, texture] : assetToTexture)
    ReleaseTexture(texture);
  for (auto &[assetId, byIndex] : modelToTextures)
    for (auto &[index, texture] : byIndex)
      ReleaseTexture(texture);
  // Render targets rather than textures, so these carry a full set of views apiece -- shader
  // resource, render target, depth stencil -- and were being dropped with all of them.
  for (auto &[assetId, target] : assetToPreviewTarget)
    ReleaseTarget(target);
  ReleaseMaterialTable(fallbackMaterial);
  fallbackMaterial = {};
  fallbackMaterialTable = {};
  // Every table, not just the map holding them. Clearing the map alone loses the indices, and with
  // them the only record of which descriptors the heap had handed out -- so a backend that was torn
  // down and set up again in one session started from a heap that was already partly spoken for.
  for (auto &[assetId, byIndex] : materialTables)
    for (auto &[index, material] : byIndex)
      ReleaseMaterialTable(material);
  materialTables.clear();
  assetToPreviewTarget.clear();
  ReleaseComputeTexture(skyboxCubemap);
  ReleaseComputeTexture(irradianceMap);
  ReleaseComputeTexture(prefilterMap);
  ReleaseComputeTexture(brdfLUT);
  ReleaseComputeTexture(dummyCubemap);
  // A heap of its own rather than a share of the visible one, so resetting it releases every
  // descriptor in it at once and the compute textures above need no individual accounting.
  computeStagingHeap.Reset();
  dummyEquirectSrv = DXDescriptorHeap::InvalidIndex;
  shBuffer.Reset();
  FreeSlot(shSrvIndex);
  FreeSlot(shUavIndex);
  // Allocated once and reused for the life of the backend, so these never grew -- but forgetting
  // them here left them out of reach of the next one, for the same reason as the material tables.
  FreeSlots(environmentTableIndex, 3);
  environmentTable = {};
  FreeSlots(fallbackEnvironmentIndex, 3);
  fallbackEnvironmentTable = {};
  FreeSlot(skyboxTableIndex);
  skyboxTable = {};
  FreeSlot(fallbackSkyboxIndex);
  fallbackSkyboxTable = {};
  // The refraction copy, which nothing else releases: it is remade only when the viewport changes
  // size, so without this it would outlive the backend that made it.
  FreeSlot(sceneColorSrvIndex);
  sceneColorTable = {};
  sceneColorCopy.Reset();
  sceneColorState = D3D12_RESOURCE_STATE_COMMON;
  sceneColorWidth = 0;
  sceneColorHeight = 0;
  sceneColorFormat = DXGI_FORMAT_UNKNOWN;
  environmentAsset = {};
  environmentReady = false;
  instanceBuffer.Reset();
  instanceData = nullptr;
  instanceCapacity = 0;
  instanceCursor = 0;
  frameConstantBuffer.Reset();
  frameConstantData = nullptr;
  frameConstantSlots = 0;
  frameConstantCursor = 0;
  retiredUploadBuffers.clear();
  // Drained rather than dropped. The material tables above went through `ReleaseMaterialTable`,
  // which retires a range instead of freeing it, and the drain that normally collects those is in
  // `Reset` -- which a backend being torn down never reaches. Clearing this without freeing would
  // have thrown away the heap capacity these fixes exist to reclaim.
  if (context)
    for (const auto &retired : retiredRanges)
      context->GetSRVHeap().FreeRange(retired.first, retired.count);
  retiredRanges.clear();
  spotShadowCount = 0;
  assetToMesh.clear();
  assetToPreview.clear();
  assetToTexture.clear();
  modelToMeshes.clear();
  modelToTextures.clear();
}
auto DXRenderer::LoadAsset(const AssetID) -> void {}
auto DXRenderer::LoadAssets(const AssetType) -> void {}
auto DXRenderer::EnsureSceneResources(Scene &scene) -> void {
  KUKI_PROFILE_SCOPE("EnsureSceneResources");
  DrainPendingUploads();
  EnsureDummyTexture();
  EnsureComputeFallbacks();
  scene.ForEachEntity<MeshHandle>([&](const EntityID, const MeshHandle *handle) {
    auto residentId = handle->assetId;
    if (!app.GetAsset<MeshAsset>(residentId))
      if (auto fallback = app.GetAsset("Cube"); fallback)
        residentId = fallback->id;
    EnsureMesh(residentId);
    if (auto meshAsset = app.GetAsset<MeshAsset>(residentId); meshAsset)
      if (auto materialAsset = app.GetAsset<MaterialAsset>(meshAsset->material); materialAsset)
        for (const auto textureId : materialAsset->textures)
          EnsureTexture(textureId);
  });
  scene.ForEachEntity<ModelMeshHandle>([&](const EntityID, const ModelMeshHandle *handle) {
    if (app.IsAssetLoaded(handle->modelAssetId))
      EnsureModelMesh(handle->modelAssetId, handle->meshIndex);
  });
  scene.ForEachEntity<ModelMaterialHandle>([&](const EntityID, const ModelMaterialHandle *handle) {
    if (!app.IsAssetLoaded(handle->modelAssetId))
      return;
    for (const auto textureIndex : handle->textureIndices)
      EnsureModelTexture(handle->modelAssetId, textureIndex);
  });
  scene.ForEachEntity<SkyboxHandle>([&](const EntityID, const SkyboxHandle *handle) {
    if (!handle->assetId || !app.IsAssetLoaded(handle->assetId))
      return;
    auto texture = EnsureTexture(handle->assetId);
    if (!texture || (environmentReady && environmentAsset == handle->assetId))
      return;
    auto textureAsset = app.GetAsset<TextureAsset>(handle->assetId);
    if (!textureAsset)
      return;
    if (!BuildEnvironmentMaps(handle->assetId, *texture)) {
      environmentReady = false;
      spdlog::warn("[DXRenderer] could not precompute image-based lighting, falling back to the analytic sky");
    }
  });
}
auto DXRenderer::LoadScene(Scene &scene) -> void {
  // Picked up once a frame here rather than read in the pass that needs it, because the post chain
  // is handed target names and nothing else. A scene with no camera keeps whatever was last set,
  // which is the same thing it draws with.
  if (const auto camera = scene.GetCamera(); camera)
    exposure = camera->GetExposureStops();
  EnsureSceneResources(scene);
  std::vector<std::pair<EntityID, DXMaterial>> pending;
  const auto Resolve = [&](const AssetID assetId, const size_t index, const MaterialFallback &fallback, const MaterialType type, const std::array<const DXTexture *, MATERIAL_TEXTURE_SLOTS> &textures) -> DXMaterial {
    auto &shared = materialTables[assetId][index];
    shared.fallback = fallback;
    shared.type = type;
    if (!shared.textureTableGPU)
      BuildMaterialTable(shared, textures);
    return shared;
  };
  const auto Place = [](std::array<const DXTexture *, MATERIAL_TEXTURE_SLOTS> &textures, const DXTexture *texture) {
    if (!texture || !*texture)
      return;
    if (const auto slot = static_cast<uint32_t>(texture->content); slot < MATERIAL_TEXTURE_SLOTS)
      textures[slot] = texture;
  };
  scene.ForEachEntity<MaterialHandle>([&](const EntityID id, const MaterialHandle *handle) {
    auto materialAsset = app.GetAsset<MaterialAsset>(handle->assetId);
    if (!materialAsset)
      return;
    std::array<const DXTexture *, MATERIAL_TEXTURE_SLOTS> textures{};
    for (const auto textureId : materialAsset->textures)
      Place(textures, EnsureTexture(textureId));
    pending.emplace_back(id, Resolve(handle->assetId, 0, materialAsset->fallback, materialAsset->type, textures));
  });
  scene.ForEachEntity<ModelMaterialHandle>([&](const EntityID id, const ModelMaterialHandle *handle) {
    const auto modelAssetId = handle->modelAssetId;
    const auto materialIndex = handle->materialIndex;
    if (!app.IsAssetLoaded(modelAssetId))
      return;
    auto modelAsset = app.GetAsset<ModelAsset>(modelAssetId);
    if (!modelAsset || materialIndex >= modelAsset->materials.size())
      return;
    const auto &modelMaterial = modelAsset->materials[materialIndex];
    std::array<const DXTexture *, MATERIAL_TEXTURE_SLOTS> textures{};
    for (const auto textureIndex : modelMaterial.textures)
      Place(textures, EnsureModelTexture(modelAssetId, textureIndex));
    pending.emplace_back(id, Resolve(modelAssetId, materialIndex, modelMaterial.fallback, modelMaterial.type, textures));
  });
  for (const auto &[id, value] : pending) {
    auto material = scene.GetEntityComponent<DXMaterial>(id);
    if (!material) {
      scene.AddEntityComponent<DXMaterial>(id);
      material = scene.GetEntityComponent<DXMaterial>(id);
    }
    if (material)
      *material = value;
  }
}
auto DXRenderer::EnsurePickResources(const DXRenderTarget &target) -> bool {
  auto context = GetContext();
  if (!context || !target.idResource)
    return false;
  if (pickResolveTexture && pickResolveWidth == target.desc.width && pickResolveHeight == target.desc.height)
    return true;
  auto *device = context->GetDevice();
  context->WaitForGPU();
  pickResolveTexture.Reset();
  auto resolveDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, static_cast<UINT64>(target.desc.width), static_cast<UINT>(target.desc.height), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_NONE);
  const auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  if (DXFailed(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &resolveDesc, D3D12_RESOURCE_STATE_RESOLVE_DEST, nullptr, IID_PPV_ARGS(&pickResolveTexture)), "CreateCommittedResource for pick resolve"))
    return false;
  if (!pickReadbackBuffer) {
    const auto readbackHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    if (DXFailed(device->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&pickReadbackBuffer)), "CreateCommittedResource for pick readback"))
      return false;
  }
  pickResolveWidth = target.desc.width;
  pickResolveHeight = target.desc.height;
  return true;
}
auto DXRenderer::PickEntity(const int x, const int y) -> EntityID {
  auto context = GetContext();
  if (!context)
    return EntityID::Invalid;
  auto *commandList = context->GetCommandList();
  if (!commandList)
    return EntityID::Invalid;
  DXRenderTarget *pickTarget{};
  for (auto &[name, target] : nameToTarget)
    if (target.idResource && target.desc.pickingBuffer) {
      pickTarget = &target;
      break;
    }
  if (!pickTarget)
    return EntityID::Invalid;
  if (x < 0 || y < 0 || x >= pickTarget->desc.width || y >= pickTarget->desc.height)
    return EntityID::Invalid;
  if (!EnsurePickResources(*pickTarget))
    return EntityID::Invalid;
  const auto idToResolveSource = CD3DX12_RESOURCE_BARRIER::Transition(pickTarget->idResource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
  commandList->ResourceBarrier(1, &idToResolveSource);
  commandList->ResolveSubresource(pickResolveTexture.Get(), 0, pickTarget->idResource.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
  const auto idBack = CD3DX12_RESOURCE_BARRIER::Transition(pickTarget->idResource.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
  const auto resolveToCopySource = CD3DX12_RESOURCE_BARRIER::Transition(pickResolveTexture.Get(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
  commandList->ResourceBarrier(1, &idBack);
  commandList->ResourceBarrier(1, &resolveToCopySource);
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  footprint.Offset = 0;
  footprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  footprint.Footprint.Width = 1;
  footprint.Footprint.Height = 1;
  footprint.Footprint.Depth = 1;
  footprint.Footprint.RowPitch = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
  const CD3DX12_TEXTURE_COPY_LOCATION destination(pickReadbackBuffer.Get(), footprint);
  const CD3DX12_TEXTURE_COPY_LOCATION source(pickResolveTexture.Get(), 0);
  const CD3DX12_BOX box(x, y, x + 1, y + 1);
  commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, &box);
  const auto resolveBack = CD3DX12_RESOURCE_BARRIER::Transition(pickResolveTexture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST);
  commandList->ResourceBarrier(1, &resolveBack);
  context->FlushCommandList();
  void *mapped{};
  const CD3DX12_RANGE readRange(0, 4);
  if (DXFailed(pickReadbackBuffer->Map(0, &readRange, &mapped), "Map pick readback"))
    return EntityID::Invalid;
  const auto *pixel = static_cast<const uint8_t *>(mapped);
  const auto value = static_cast<uint32_t>(pixel[0]) | (static_cast<uint32_t>(pixel[1]) << 8) | (static_cast<uint32_t>(pixel[2]) << 16);
  const CD3DX12_RANGE writeRange(0, 0);
  pickReadbackBuffer->Unmap(0, &writeRange);
  if (value == ENTITY_ID_ENCODED_INVALID)
    return EntityID::Invalid;
  return static_cast<EntityID>(value);
}
auto DXRenderer::EnsurePreviewMaterial(const AssetID assetId, const MaterialAsset &materialAsset) -> const DXMaterial * {
  auto &shared = materialTables[assetId][0];
  shared.fallback = materialAsset.fallback;
  shared.type = materialAsset.type;
  if (shared.textureTableGPU)
    return &shared;
  std::array<const DXTexture *, MATERIAL_TEXTURE_SLOTS> textures{};
  for (const auto textureId : materialAsset.textures)
    if (auto texture = EnsureTexture(textureId); texture && *texture)
      if (const auto slot = static_cast<uint32_t>(texture->content); slot < MATERIAL_TEXTURE_SLOTS)
        textures[slot] = texture;
  BuildMaterialTable(shared, textures);
  return &shared;
}
auto DXRenderer::RenderPreview(const AssetID assetId, const std::vector<std::pair<const DXMesh *, const DXMaterial *>> &draws, const BoundingBox &bounds) -> RenderTarget * {
  if (auto it = assetToPreviewTarget.find(assetId); it != assetToPreviewTarget.end())
    return it->second.resource ? &it->second : nullptr;
  auto context = GetContext();
  if (!context || draws.empty())
    return nullptr;
  auto *commandList = context->GetCommandList();
  auto fallbackTexture = EnsureDummyTexture();
  if (!commandList || !fallbackTexture || !EnsureComputeFallbacks())
    return nullptr;
  auto &target = assetToPreviewTarget[assetId];
  const TargetDescription desc{.format = TargetFormat::RGBA16, .width = previewSize, .height = previewSize};
  if (!AllocateTarget(target, desc, "Preview " + app.GetAssetName(assetId)) || !EnsureDepthBuffer(target))
    return nullptr;
  const auto pipeline = pipelines.GetScenePipeline(context->GetDevice(), TargetFormatToRTVDXGI(desc.format), 1);
  if (!pipeline || !*pipeline)
    return nullptr;
  Camera camera{};
  camera.Frame(bounds);
  Light light{};
  light.SetRotation(glm::quat(glm::vec3(.52f, .0f, .52f)));
  DXFrameConstants frameConstants{};
  const auto viewProjection = camera.transform.projection * camera.transform.view;
  memcpy(frameConstants.viewProjection, glm::value_ptr(viewProjection), sizeof(frameConstants.viewProjection));
  memcpy(frameConstants.viewPosition, glm::value_ptr(camera.position), sizeof(float) * 3);
  memcpy(frameConstants.directionalDirection, glm::value_ptr(light.forward), sizeof(float) * 3);
  memcpy(frameConstants.directionalAmbient, glm::value_ptr(light.ambient), sizeof(float) * 3);
  memcpy(frameConstants.directionalDiffuse, glm::value_ptr(light.diffuse), sizeof(float) * 3);
  memcpy(frameConstants.directionalSpecular, glm::value_ptr(light.specular), sizeof(float) * 3);
  frameConstants.directionalIntensity[0] = light.intensity;
  frameConstants.counts[2] = 1u;
  frameConstants.flags[0] = 1u;
  frameConstants.flags[2] = environmentReady ? 1u : 0u;
  const auto frameConstantAddress = UploadFrameConstants(frameConstants);
  if (!frameConstantAddress)
    return nullptr;
  Transition(target, D3D12_RESOURCE_STATE_RENDER_TARGET);
  const auto rtv = context->GetRTVHeap().GetCPUHandle(target.rtvIndex);
  const auto dsv = context->GetDSVHeap().GetCPUHandle(target.depthDsvIndex);
  commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
  commandList->ClearRenderTargetView(rtv, TARGET_CLEAR_COLOR, 0, nullptr);
  commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
  const auto viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<float>(desc.width), static_cast<float>(desc.height));
  const auto scissor = CD3DX12_RECT(0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height));
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  commandList->SetGraphicsRootSignature(pipeline->rootSignature.Get());
  commandList->SetPipelineState(pipeline->pipelineState.Get());
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commandList->SetGraphicsRootConstantBufferView(1, frameConstantAddress);
  commandList->SetGraphicsRootDescriptorTable(3, fallbackTexture->srvGPU);
  commandList->SetGraphicsRootDescriptorTable(4, dummyArraySrvGPU.ptr ? dummyArraySrvGPU : fallbackTexture->srvGPU);
  commandList->SetGraphicsRootDescriptorTable(7, environmentReady ? environmentTable : fallbackEnvironmentTable);
  commandList->SetGraphicsRootDescriptorTable(11, fallbackTexture->srvGPU);
  BindProbeVolume(frameConstantAddress);
  DXInstanceData instance{};
  memcpy(instance.model, glm::value_ptr(glm::mat4(1.f)), sizeof(instance.model));
  const auto instanceAddress = AllocateInstances(std::span(&instance, 1));
  if (!instanceAddress)
    return nullptr;
  commandList->SetGraphicsRootShaderResourceView(5, instanceAddress);
  commandList->SetGraphicsRootShaderResourceView(6, instanceAddress);
  for (const auto &[mesh, material] : draws) {
    if (!mesh || !*mesh || mesh->skinned)
      continue;
    DXSceneConstants constants{};
    const auto fallback = material ? material->fallback : MaterialFallback{};
    memcpy(constants.albedo, glm::value_ptr(fallback.albedo), sizeof(constants.albedo));
    memcpy(constants.specular, glm::value_ptr(fallback.specular), sizeof(constants.specular));
    memcpy(constants.emissive, glm::value_ptr(fallback.emissive), sizeof(constants.emissive));
    constants.surface[0] = fallback.metalness;
    constants.surface[1] = fallback.occlusion;
    constants.surface[2] = fallback.roughness;
    memcpy(constants.attenuation, glm::value_ptr(fallback.attenuation), sizeof(constants.attenuation));
    constants.volume[0] = fallback.transmission;
    constants.volume[1] = fallback.thickness;
    constants.volume[2] = fallback.ior;
    const auto hasTable = material && material->textureTableGPU;
    constants.textureMask = hasTable ? static_cast<uint32_t>(fallback.textureMask.to_ulong()) : 0u;
    constants.unlit = material && material->type == MaterialType::Unlit ? 1u : 0u;
    commandList->SetGraphicsRoot32BitConstants(0, sizeof(DXSceneConstants) / sizeof(uint32_t), &constants, 0);
    D3D12_GPU_DESCRIPTOR_HANDLE materialTable{hasTable ? material->textureTableGPU : fallbackMaterialTable.ptr};
    commandList->SetGraphicsRootDescriptorTable(2, materialTable);
    commandList->IASetVertexBuffers(0, 1, &mesh->vertexBufferView);
    if (mesh->indexCount > 0) {
      commandList->IASetIndexBuffer(&mesh->indexBufferView);
      commandList->DrawIndexedInstanced(mesh->indexCount, 1, 0, 0, 0);
    } else
      commandList->DrawInstanced(mesh->vertexCount, 1, 0, 0);
  }
  Transition(target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  spdlog::info("[DXRenderer] created preview for asset: {}", app.GetAssetName(assetId));
  return &target;
}
auto DXRenderer::PreviewAsset(const AssetID assetId) -> RenderTarget * {
  if (auto textureAsset = app.GetAsset<TextureAsset>(assetId); textureAsset) {
    auto texture = EnsureTexture(assetId);
    if (!texture)
      return nullptr;
    auto &view = assetToPreview[assetId];
    view.srvGPU = texture->srvGPU;
    view.desc.width = textureAsset->texture.width;
    view.desc.height = textureAsset->texture.height;
    return &view;
  }
  if (auto meshAsset = app.GetAsset<MeshAsset>(assetId); meshAsset) {
    auto mesh = EnsureMesh(assetId);
    if (!mesh)
      return nullptr;
    const DXMaterial *material{};
    if (auto materialAsset = app.GetAsset<MaterialAsset>(meshAsset->material); materialAsset)
      material = EnsurePreviewMaterial(materialAsset->id, *materialAsset);
    return RenderPreview(assetId, {{mesh, material}}, meshAsset->bounds);
  }
  if (auto materialAsset = app.GetAsset<MaterialAsset>(assetId); materialAsset) {
    auto sphere = app.GetAsset("Sphere");
    if (!sphere)
      return nullptr;
    auto meshAsset = app.GetAsset<MeshAsset>(sphere->id);
    auto mesh = EnsureMesh(sphere->id);
    if (!mesh || !meshAsset)
      return nullptr;
    return RenderPreview(assetId, {{mesh, EnsurePreviewMaterial(assetId, *materialAsset)}}, meshAsset->bounds);
  }
  if (auto modelAsset = app.GetAsset<ModelAsset>(assetId); modelAsset) {
    if (modelAsset->nodes.empty())
      return nullptr;
    std::vector<std::pair<const DXMesh *, const DXMaterial *>> draws;
    for (size_t i = 0; i < modelAsset->meshes.size(); ++i) {
      auto mesh = EnsureModelMesh(assetId, i);
      if (!mesh)
        continue;
      const DXMaterial *material{};
      if (const auto materialIndex = modelAsset->meshes[i].material; materialIndex < modelAsset->materials.size()) {
        std::array<const DXTexture *, MATERIAL_TEXTURE_SLOTS> textures{};
        for (const auto textureIndex : modelAsset->materials[materialIndex].textures)
          if (auto texture = EnsureModelTexture(assetId, textureIndex); texture && *texture)
            if (const auto slot = static_cast<uint32_t>(texture->content); slot < MATERIAL_TEXTURE_SLOTS)
              textures[slot] = texture;
        auto &shared = materialTables[assetId][materialIndex];
        shared.fallback = modelAsset->materials[materialIndex].fallback;
        shared.type = modelAsset->materials[materialIndex].type;
        if (!shared.textureTableGPU)
          BuildMaterialTable(shared, textures);
        material = &shared;
      }
      draws.emplace_back(mesh, material);
    }
    return RenderPreview(assetId, draws, modelAsset->nodes[0].bounds);
  }
  return nullptr;
}
auto DXRenderer::GetPreviewSize() const -> int {
  return previewSize;
}
auto DXRenderer::SetPreviewSize(const int size) -> void {
  previewSize = size;
}
auto DXRenderer::SetResolution(const int width, const int height) -> void {
  if (width <= 0 || height <= 0)
    return;
  if (width == screenWidth && height == screenHeight)
    return;
  screenWidth = width;
  screenHeight = height;
}
auto DXRenderer::PresentTarget(const std::string &name) -> void {
  auto *context = GetContext();
  if (!context)
    return;
  auto *commandList = context->GetCommandList();
  if (!commandList)
    return;
  auto it = nameToTarget.find(name);
  if (it == nameToTarget.end())
    return;
  auto &source = it->second;
  // Multisampled or unreadable sources are not resolved here on purpose. The graph's final output
  // is single-sampled and has an SRV -- the tone mapping pass wrote it -- so anything else means
  // the caller asked for an intermediate, and drawing a wrong picture would read as a broken frame
  // rather than as a request that could not be served.
  if (!source.resource || source.desc.samples > 1 || source.srvIndex == DXDescriptorHeap::InvalidIndex)
    return;
  const auto pipeline = pipelines.GetPostPipeline(context->GetDevice(), context->GetBackBufferFormat(), "PSCopy");
  if (!pipeline || !*pipeline)
    return;
  const auto [surfaceWidth, surfaceHeight] = context->GetSurfaceSize();
  if (surfaceWidth <= 0 || surfaceHeight <= 0)
    return;
  // The back buffer is already in `RENDER_TARGET` -- `DXContext::BeginFrame` put it there and
  // `Present` transitions it back -- so only the source needs a barrier.
  Transition(source, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  const auto rtv = context->GetBackBufferRTV();
  commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
  const auto viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<float>(surfaceWidth), static_cast<float>(surfaceHeight));
  const auto scissor = CD3DX12_RECT(0, 0, static_cast<LONG>(surfaceWidth), static_cast<LONG>(surfaceHeight));
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  commandList->SetGraphicsRootSignature(pipeline->rootSignature.Get());
  commandList->SetPipelineState(pipeline->pipelineState.Get());
  // `PSCopy` reads none of these, but the root signature declares them and a table left unbound is
  // undefined behaviour on some drivers rather than a no-op.
  DXPostConstants constants{};
  constants.texelSize[0] = surfaceWidth > 0 ? 1.f / surfaceWidth : 0.f;
  constants.texelSize[1] = surfaceHeight > 0 ? 1.f / surfaceHeight : 0.f;
  commandList->SetGraphicsRoot32BitConstants(0, sizeof(DXPostConstants) / sizeof(uint32_t), &constants, 0);
  commandList->SetGraphicsRootDescriptorTable(1, source.srvGPU);
  commandList->SetGraphicsRootDescriptorTable(2, source.srvGPU);
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commandList->IASetVertexBuffers(0, 0, nullptr);
  commandList->DrawInstanced(3, 1, 0, 0);
}
auto DXRenderer::Reset() -> void {
  instanceCursor = 0;
  frameConstantCursor = 0;
  ++uploadEpoch;
  // A buffer retired during frame N can still be referenced by the command lists of frames N and
  // the ones already in flight, so it is held for as many frames as the context keeps in flight
  // before being let go.
  std::erase_if(retiredUploadBuffers, [this](const auto &retired) {
    return retired.first + DX_FRAME_COUNT < uploadEpoch;
  });
  auto context = GetContext();
  if (!context)
    return;
  auto &srvHeap = context->GetSRVHeap();
  std::erase_if(retiredRanges, [&](const DXRetiredRange &retired) {
    if (retired.epoch + DX_FRAME_COUNT >= uploadEpoch)
      return false;
    srvHeap.FreeRange(retired.first, retired.count);
    return true;
  });
}
auto DXRenderer::ReleaseAllTargets() -> void {
  if (auto context = GetContext(); context) {
    if (context->GetCommandList())
      context->FlushCommandList();
    else
      context->WaitForGPU();
  }
  for (auto &[name, target] : nameToTarget)
    ReleaseTarget(target);
  nameToTarget.clear();
  nameToId.clear();
}
} // namespace kuki
#endif
