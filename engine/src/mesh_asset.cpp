#include <mesh_asset.hpp>
namespace kuki {
MeshAsset::MeshAsset(AssetID id, std::string name)
  : Asset(std::in_place_type<MeshAsset>, id, std::move(name)) {}
} // namespace kuki
