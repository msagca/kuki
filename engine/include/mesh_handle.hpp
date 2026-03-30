#pragma once
#include <id.hpp>
namespace kuki {
struct SceneMeshHandle {
  AssetID sceneAssetId{};
  EntityID resourceId{};
  size_t meshIndex{};
};
struct MeshHandle {
  AssetID assetId{};
  EntityID resourceId{};
};
} // namespace kuki
