#include <skybox_asset.hpp>
namespace kuki {
SkyboxAsset::SkyboxAsset(const AssetID id, std::string name)
  : Asset(std::in_place_type<SkyboxAsset>, id, std::move(name)) {}
} // namespace kuki
