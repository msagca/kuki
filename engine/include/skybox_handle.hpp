#pragma once
#include <id.hpp>
namespace kuki {
struct SkyboxHandle {
  AssetID assetId{};
  EntityID prefabId{};
  EntityID resourceId{};
};
} // namespace kuki
