#pragma once
#include <id.hpp>
#include <kuki_engine_export.h>
namespace kuki {
/// @brief Placeholder for a mesh in a scene hierarchy
struct KUKI_ENGINE_API MeshHandle {
  /// @brief ID of the `SceneAsset`
  AssetID assetId{AssetID::Invalid};
  /// @brief An index into the `meshes` array of the `SceneAsset`
  size_t arrayIndex{};
};
} // namespace kuki
