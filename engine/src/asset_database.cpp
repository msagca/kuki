#include <asset_database.hpp>
#include <asset_metadata.hpp>
#include <asset_type.hpp>
#include <id.hpp>
#include <material_asset.hpp>
#include <mesh_asset.hpp>
#include <scene_asset.hpp>
#include <shader_asset.hpp>
#include <skybox_asset.hpp>
#include <texture_asset.hpp>
namespace kuki {
const std::unordered_map<std::type_index, AssetType> AssetDatabase::typeIndexToAssetType = {
  {typeid(MaterialAsset), AssetType::Material},
  {typeid(MeshAsset), AssetType::Mesh},
  {typeid(SceneAsset), AssetType::Scene},
  {typeid(ShaderAsset), AssetType::Shader},
  {typeid(SkyboxAsset), AssetType::Skybox},
  {typeid(TextureAsset), AssetType::Texture}};
auto AssetDatabase::Clear() -> void {
  idToMetadata.clear();
  pathToId.clear();
}
auto AssetDatabase::GetID(const std::string &name) const -> AssetID {
  auto ids = nameToId.equal_range(name);
  if (auto it = ids.first; it != ids.second)
    return it->second;
  return AssetID::Invalid;
}
auto AssetDatabase::GetName(const AssetID id) const -> std::string {
  if (auto it = idToMetadata.find(id); it != idToMetadata.end())
    return it->second.name;
  return "";
}
auto AssetDatabase::GetPath(const AssetID id) const -> std::filesystem::path {
  if (auto it = idToMetadata.find(id); it != idToMetadata.end())
    return it->second.path;
  return std::filesystem::path{};
}
auto AssetDatabase::GetStatus(const AssetID id) const -> AssetStatus {
  if (auto it = idToMetadata.find(id); it != idToMetadata.end())
    return it->second.status;
  return AssetStatus::Unregistered;
}
auto AssetDatabase::GetType(const AssetID id) const -> AssetType {
  if (auto it = idToMetadata.find(id); it != idToMetadata.end())
    return it->second.type;
  return AssetType::Texture;
}
auto AssetDatabase::IsRegistered(const AssetID id) const -> bool {
  return idToMetadata.contains(id);
}
auto AssetDatabase::SetStatus(const AssetID id, const AssetStatus status) -> bool {
  if (status == AssetStatus::Unregistered)
    return false;
  if (auto it = idToMetadata.find(id); it != idToMetadata.end()) {
    it->second.status = status;
    return true;
  }
  return false;
}
auto AssetDatabase::Unregister(const AssetID id) -> bool {
  if (auto it = idToMetadata.find(id); it != idToMetadata.end()) {
    pathToId.erase(it->second.path);
    auto ids = nameToId.equal_range(it->second.name);
    for (auto it = ids.first; it != ids.second; ++it)
      if (it->second == id) {
        nameToId.erase(it);
        break;
      }
  }
  return idToMetadata.erase(id) > 0;
}
} // namespace kuki
