#pragma once
#include <id.hpp>
namespace kuki {
struct SceneMaterialHandle {
  AssetID sceneAssetId{};
  EntityID resourceId{};
  size_t materialIndex;
  std::vector<size_t> textureIndices;
};
struct MaterialHandle {
  AssetID assetId{};
  EntityID resourceId{};
};
} // namespace kuki
