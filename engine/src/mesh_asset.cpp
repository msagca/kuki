#include <asset.hpp>
#include <id.hpp>
#include <mesh_asset.hpp>
#include <string>
#include <utility>
namespace kuki {
MeshAsset::MeshAsset(AssetID id)
  : Asset(std::in_place_type<MeshAsset>, id) {}
} // namespace kuki
