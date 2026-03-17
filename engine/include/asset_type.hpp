#pragma once
#include <bitset>
#include <cstdint>
namespace kuki {
enum class AssetType : uint8_t {
  Scene,
  Shader,
  Skybox,
  Unknown,
};
using AssetMask = std::bitset<static_cast<uint8_t>(AssetType::Unknown) + 1>;
} // namespace kuki
