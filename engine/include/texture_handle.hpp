#pragma once
#include <id.hpp>
namespace kuki {
struct TextureHandle {
  AssetID assetId{};
  EntityID resourceId{};
};
} // namespace kuki
