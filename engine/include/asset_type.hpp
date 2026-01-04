#pragma once
#include <bitset>
#include <cstdint>
namespace kuki {
enum class AssetType : uint8_t {
  Audio,
  Material,
  Mesh,
  Scene,
  Script,
  Shader,
  Skybox,
  Texture,
  Unknown,
};
using AssetMask = std::bitset<static_cast<uint8_t>(AssetType::Unknown) + 1>;
} // namespace kuki
