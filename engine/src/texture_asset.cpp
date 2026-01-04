#include <texture_asset.hpp>
namespace kuki {
TextureAsset::TextureAsset(const AssetID id)
  : Asset(std::in_place_type<TextureAsset>, id) {}
} // namespace kuki
