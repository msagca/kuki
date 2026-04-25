#pragma once
#include <bitset>
#include <cstdint>
namespace kuki {
enum class AssetType : uint8_t {
  Material,
  Mesh,
  Scene,
  Shader,
  Texture
};
using AssetMask = std::bitset<static_cast<uint8_t>(AssetType::Texture) + 1>;
} // namespace kuki
