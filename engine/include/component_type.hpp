#pragma once
#include <bitset>
namespace kuki {
enum class ComponentType : uint8_t {
  BoneData,
  Camera,
  GLMaterial,
  GLMesh,
  GLSkybox,
  GLTexture,
  Light,
  MaterialHandle,
  MeshHandle,
  SkyboxHandle,
  Transform,
  Unknown
};
using ComponentMask = std::bitset<static_cast<uint8_t>(ComponentType::Unknown) + 1>;
} // namespace kuki
