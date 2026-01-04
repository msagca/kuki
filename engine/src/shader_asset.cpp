#include <shader_asset.hpp>
namespace kuki {
ShaderAsset::ShaderAsset(const AssetID id)
  : Asset(std::in_place_type<ShaderAsset>, id) {}
} // namespace kuki
