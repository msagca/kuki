#include <mesh_asset.hpp>
namespace kuki {
MeshAsset::MeshAsset(const AssetID id)
  : Asset(std::in_place_type<MeshAsset>, id) {}
} // namespace kuki
