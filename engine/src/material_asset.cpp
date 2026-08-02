#include <asset.hpp>
#include <id.hpp>
#include <material_asset.hpp>
#include <string>
#include <utility>
namespace kuki {
MaterialAsset::MaterialAsset(AssetID id)
  : Asset(std::in_place_type<MaterialAsset>, id) {}
} // namespace kuki
