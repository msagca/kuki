#pragma once
#include <id.hpp>
namespace kuki {
struct ModelMeshHandle {
  AssetID modelAssetId{};
  EntityID resourceId{};
  size_t meshIndex{};
};
struct MeshHandle {
  AssetID assetId{};
  EntityID resourceId{};
};
} // namespace kuki
