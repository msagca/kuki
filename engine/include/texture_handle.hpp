#pragma once
#include <id.hpp>
#include <kuki_engine_export.h>
namespace kuki {
/// @brief Placeholder for a texture in a scene hierarchy
struct KUKI_ENGINE_API TextureHandle {
  /// @brief ID of the `SceneAsset`
  AssetID assetId{AssetID::Invalid};
  /// @brief An index into the `textures` array of the `SceneAsset`
  size_t arrayIndex{};
};
} // namespace kuki
