#include <asset_database.hpp>
#include <asset_metadata.hpp>
#include <asset_type.hpp>
#include <scene_asset.hpp>
#include <shader_asset.hpp>
#include <skybox_asset.hpp>
namespace kuki {
const std::unordered_map<std::type_index, AssetType> AssetDatabase::typeIndexToAssetType = {
  {typeid(SceneAsset), AssetType::Scene},
  {typeid(ShaderAsset), AssetType::Shader},
  {typeid(SkyboxAsset), AssetType::Skybox}};
auto AssetDatabase::Unregister(const AssetID id) -> bool {
  if (auto it = idToMetadata.find(id); it != idToMetadata.end())
    pathToId.erase(it->second.path);
  return idToMetadata.erase(id) > 0;
}
auto AssetDatabase::Reset() -> void {
  idToMetadata.clear();
  pathToId.clear();
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
  return AssetType::Unknown;
}
auto AssetDatabase::GetName(const AssetID id) const -> std::string {
  if (auto it = idToMetadata.find(id); it != idToMetadata.end())
    return it->second.name;
  return {};
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
} // namespace kuki
