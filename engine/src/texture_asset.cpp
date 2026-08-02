#include <asset.hpp>
#include <id.hpp>
#include <string>
#include <texture_asset.hpp>
#include <utility>
namespace kuki {
TextureAsset::TextureAsset(AssetID id)
  : Asset(std::in_place_type<TextureAsset>, id) {}
} // namespace kuki
