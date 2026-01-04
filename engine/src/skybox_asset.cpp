#include <skybox_asset.hpp>
namespace kuki {
SkyboxAsset::SkyboxAsset(const AssetID id)
  : Asset(std::in_place_type<SkyboxAsset>, id) {}
} // namespace kuki
