#pragma once
#include <bitset>
namespace kuki {
enum class ComponentType : uint8_t {
  Animator,
  BoneData,
  BoundingBox,
  Camera,
  DXMaterial,
  GLBuffer,
  GLComputeShader,
  GLLitShader,
  GLMaterial,
  GLMesh,
  GLSkybox,
  GLRenderTarget,
  GLTexture,
  GLUnlitShader,
  IndirectLighting,
  Light,
  MaterialHandle,
  MeshHandle,
  ModelMaterialHandle,
  ModelMeshHandle,
  Script,
  Skeleton,
  SkyboxHandle,
  TextureHandle,
  Transform
};
using ComponentMask = std::bitset<static_cast<uint8_t>(ComponentType::Transform) + 1>;
static_assert(static_cast<uint8_t>(ComponentType::GLUnlitShader) - static_cast<uint8_t>(ComponentType::GLBuffer) == 8, "GL component types (GLBuffer..GLUnlitShader) must stay contiguous -- Component::IsGL relies on this range");
} // namespace kuki
