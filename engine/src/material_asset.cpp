#include <material_asset.hpp>
namespace kuki {
MaterialAsset::MaterialAsset(AssetID id, std::string name)
  : Asset(std::in_place_type<MaterialAsset>, id, std::move(name)) {}
} // namespace kuki
