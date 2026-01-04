#include <material_asset.hpp>
namespace kuki {
MaterialAsset::MaterialAsset(const AssetID id)
  : Asset(std::in_place_type<MaterialAsset>, id) {}
} // namespace kuki
