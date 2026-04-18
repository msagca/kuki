#pragma once
#include <bitset>
namespace kuki {
enum class ComponentType : uint8_t {
  BoneData,
  BoundingBox,
  Camera,
  GLBuffer,
  GLComputeShader,
  GLLitShader,
  GLMaterial,
  GLMesh,
  GLSkybox,
  GLRenderTarget,
  GLTexture,
  GLUnlitShader,
  Light,
  MaterialHandle,
  MeshHandle,
  SceneMaterialHandle,
  SceneMeshHandle,
  Script,
  SkyboxHandle,
  TextureHandle,
  Transform
};
using ComponentMask = std::bitset<static_cast<uint8_t>(ComponentType::Transform) + 1>;
} // namespace kuki
