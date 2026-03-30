#include <texture_asset.hpp>
namespace kuki {
TextureAsset::TextureAsset(AssetID id, std::string name)
  : Asset(std::in_place_type<TextureAsset>, id, std::move(name)) {}
} // namespace kuki
