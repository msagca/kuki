#pragma once
#include <asset_metadata.hpp>
#include <concepts.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <typeindex>
#include <unordered_map>
namespace kuki {
class KUKI_ENGINE_API AssetDatabase {
public:
  /// @brief Remove all entries and associated metadata
  auto Clear() -> void;
  auto GetID(const std::string &) const -> AssetID;
  auto GetName(const AssetID) const -> std::string;
  auto GetPath(const AssetID) const -> std::filesystem::path;
  auto GetStatus(const AssetID) const -> AssetStatus;
  auto GetType(const AssetID) const -> AssetType;
  auto IsRegistered(const AssetID) const -> bool;
  auto SetStatus(const AssetID, const AssetStatus) -> bool;
  /// @return `true` if the entry was found and removed, `false` if it didn't exist
  auto Unregister(const AssetID) -> bool;
  /// @brief Add an entry to the database
  /// @return Asset ID
  template <IsAsset T>
  auto Register(const std::filesystem::path &, std::string = "") -> AssetID;
  template <IsAsset T>
  auto Register(const T &) -> bool;
  /// @brief Execute a function for each asset of the specified type
  void ForEach(auto &&);
private:
  static const std::unordered_map<std::type_index, AssetType> typeIndexToAssetType;
  std::unordered_map<AssetID, AssetMetadata> idToMetadata;
  std::unordered_multimap<std::string, AssetID> nameToId;
  std::unordered_map<std::filesystem::path, AssetID> pathToId;
  template <IsAsset T>
  static auto GetAssetType() -> AssetType;
};
template <IsAsset T>
auto AssetDatabase::Register(const std::filesystem::path &path, std::string name) -> AssetID {
  if (auto it = pathToId.find(path); it != pathToId.end())
    return it->second;
  AssetMetadata metadata{
    .id = AssetID::Generate(),
    .name = name.empty() ? path.filename().stem().string() : std::move(name),
    .path = path,
    .type = GetAssetType<T>(),
    .status = AssetStatus::Registered};
  idToMetadata.emplace(metadata.id, metadata);
  nameToId.emplace(metadata.name, metadata.id);
  pathToId.emplace(path, metadata.id);
  return metadata.id;
}
template <IsAsset T>
auto AssetDatabase::Register(const T &asset) -> bool {
  if (auto it = idToMetadata.find(asset.id); it != idToMetadata.end())
    return false;
  AssetMetadata metadata{
    .id = asset.id,
    .name = asset.GetName(),
    .path = "",
    .type = GetAssetType<T>(),
    .status = AssetStatus::Loaded};
  idToMetadata.emplace(metadata.id, metadata);
  nameToId.emplace(metadata.name, metadata.id);
  return true;
}
auto AssetDatabase::ForEach(auto &&func) -> void {
  for (auto &[id, metadata] : idToMetadata)
    func(id, metadata);
}
template <IsAsset T>
auto AssetDatabase::GetAssetType() -> AssetType {
  const auto typeIndex = std::type_index(typeid(T));
  if (auto it = typeIndexToAssetType.find(typeIndex); it != typeIndexToAssetType.end())
    return it->second;
  return AssetType::Texture;
}
} // namespace kuki
