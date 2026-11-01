#pragma once
#include <bitset>
#include <cstdint>
namespace kuki {
enum class AssetType : uint8_t {
  Material,
  Mesh,
  Model,
  Shader,
  Texture,
  /// @brief An animator's states (`AnimatorController`). Last, so that the numbers saved scene
  /// manifests hold for the types before it stay what they were.
  Controller
};
using AssetMask = std::bitset<static_cast<uint8_t>(AssetType::Controller) + 1>;
} // namespace kuki
