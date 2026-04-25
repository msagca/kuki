#include <asset.hpp>
#include <asset_metadata.hpp>
#include <material_asset.hpp>
#include <mesh_asset.hpp>
#include <scene_asset.hpp>
#include <shader_asset.hpp>
#include <texture_asset.hpp>
namespace kuki {
const std::unordered_map<AssetType, std::string> Asset::typeToName = {
  {AssetType::Material, "Material"},
  {AssetType::Mesh, "Mesh"},
  {AssetType::Scene, "Scene"},
  {AssetType::Shader, "Shader"},
  {AssetType::Texture, "Texture"}};
const std::unordered_map<AssetType, std::type_index> Asset::typeToTypeIndex = {
  {AssetType::Material, typeid(MaterialAsset)},
  {AssetType::Mesh, typeid(MeshAsset)},
  {AssetType::Scene, typeid(SceneAsset)},
  {AssetType::Shader, typeid(ShaderAsset)},
  {AssetType::Texture, typeid(TextureAsset)}};
const std::unordered_map<std::type_index, AssetType> Asset::typeIndexToType = {
  {typeid(MaterialAsset), AssetType::Material},
  {typeid(MeshAsset), AssetType::Mesh},
  {typeid(SceneAsset), AssetType::Scene},
  {typeid(ShaderAsset), AssetType::Shader},
  {typeid(TextureAsset), AssetType::Texture}};
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
  return AssetType::Texture;
}
auto Asset::GetTypeIndex(const AssetType type) -> std::type_index {
  if (auto it = typeToTypeIndex.find(type); it != typeToTypeIndex.end())
    return it->second;
  return typeid(AssetType::Texture);
}
auto Asset::GetTypeName(const AssetType type) -> std::string {
  return typeToName.at(type);
}
} // namespace kuki
