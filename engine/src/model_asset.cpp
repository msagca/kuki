#include <asset.hpp>
#include <id.hpp>
#include <model_asset.hpp>
#include <string>
#include <utility>
namespace kuki {
ModelAsset::ModelAsset(AssetID id)
  : Asset(std::in_place_type<ModelAsset>, id) {}
} // namespace kuki
