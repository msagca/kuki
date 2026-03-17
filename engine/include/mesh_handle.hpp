#pragma once
#include <id.hpp>
namespace kuki {
struct MeshHandle {
  AssetID sceneAssetId{};
  EntityID resourceId{};
  size_t meshIndex{};
};
} // namespace kuki
