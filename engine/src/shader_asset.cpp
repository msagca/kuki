#include <asset.hpp>
#include <id.hpp>
#include <shader_asset.hpp>
#include <string>
#include <utility>
namespace kuki {
ShaderAsset::ShaderAsset(const AssetID id)
  : Asset(std::in_place_type<ShaderAsset>, id) {}
} // namespace kuki
