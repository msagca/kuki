#pragma once
#include <id.hpp>
namespace kuki {
struct ModelMaterialHandle {
  AssetID modelAssetId{};
  EntityID resourceId{};
  size_t materialIndex;
  std::vector<size_t> textureIndices;
};
struct MaterialHandle {
  AssetID assetId{};
  EntityID resourceId{};
};
} // namespace kuki
