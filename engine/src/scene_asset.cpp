#include <scene_asset.hpp>
namespace kuki {
SceneAsset::SceneAsset(const AssetID id)
  : Asset(std::in_place_type<SceneAsset>, id) {}
} // namespace kuki
