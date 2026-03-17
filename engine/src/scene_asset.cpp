#include <scene_asset.hpp>
namespace kuki {
SceneAsset::SceneAsset(AssetID id, std::string name)
  : Asset(std::in_place_type<SceneAsset>, id, std::move(name)) {}
} // namespace kuki
