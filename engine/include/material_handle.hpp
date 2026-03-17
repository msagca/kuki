#pragma once
#include <id.hpp>
namespace kuki {
struct MaterialHandle {
  AssetID sceneAssetId{};
  EntityID resourceId{};
  size_t materialIndex;
  std::vector<size_t> textureIndices;
};
} // namespace kuki
