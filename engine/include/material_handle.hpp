#pragma once
#include <id.hpp>
#include <kuki_engine_export.h>
#include <texture_handle.hpp>
namespace kuki {
/// @brief Placeholder for a material in a scene hierarchy
struct KUKI_ENGINE_API MaterialHandle {
  /// @brief ID of the `SceneAsset`
  AssetID assetId{AssetID::Invalid};
  /// @brief An index into the `materials` array of the `SceneAsset`
  size_t arrayIndex{};
  std::vector<TextureHandle> textureHandles;
};
} // namespace kuki
