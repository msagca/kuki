#include <asset.hpp>
#include <asset_metadata.hpp>
#include <scene_asset.hpp>
#include <shader_asset.hpp>
#include <skybox_asset.hpp>
namespace kuki {
const std::unordered_map<AssetType, std::string> Asset::typeToName = {
  {AssetType::Scene, "Scene"},
  {AssetType::Shader, "Shader"},
  {AssetType::Skybox, "Skybox"},
  {AssetType::Unknown, "Unknown"}};
const std::unordered_map<AssetType, std::type_index> Asset::typeToTypeIndex = {
  {AssetType::Scene, typeid(SceneAsset)},
  {AssetType::Shader, typeid(ShaderAsset)},
  {AssetType::Skybox, typeid(SkyboxAsset)}};
const std::unordered_map<std::type_index, AssetType> Asset::typeIndexToType = {
  {typeid(SceneAsset), AssetType::Scene},
  {typeid(ShaderAsset), AssetType::Shader},
  {typeid(SkyboxAsset), AssetType::Skybox}};
auto Asset::GetName() const -> const std::string & {
  return name;
}
auto Asset::GetType() const -> AssetType {
  return GetType(typeIndex);
}
auto Asset::GetTypeIndex() const -> std::type_index {
  return typeIndex;
}
auto Asset::GetTypeName() const -> std::string {
  return GetTypeName(GetType());
}
auto Asset::Rename(std::string newName) -> void {
  name = std::move(newName);
}
auto Asset::GetMask(const AssetType type) -> AssetMask {
  return AssetMask{}.set(static_cast<uint8_t>(type));
}
auto Asset::GetType(const std::type_index typeIndex) -> AssetType {
  if (auto it = typeIndexToType.find(typeIndex); it != typeIndexToType.end())
    return it->second;
  return AssetType::Unknown;
}
auto Asset::GetTypeIndex(const AssetType type) -> std::type_index {
  if (auto it = typeToTypeIndex.find(type); it != typeToTypeIndex.end())
    return it->second;
  return typeid(AssetType::Unknown);
}
auto Asset::GetTypeName(const AssetType type) -> std::string {
  return typeToName.at(type);
}
} // namespace kuki
