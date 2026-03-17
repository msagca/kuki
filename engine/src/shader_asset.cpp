#include <shader_asset.hpp>
namespace kuki {
ShaderAsset::ShaderAsset(const AssetID id, std::string name)
  : Asset(std::in_place_type<ShaderAsset>, id, std::move(name)) {}
} // namespace kuki
