#pragma once
#include <asset_metadata.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <span>
#include <unordered_map>
namespace kuki {
class KUKI_ENGINE_API AssetDatabase {
public:
  /// @brief Add an entry to the database
  /// @return New or existing asset ID
  auto Register(const std::filesystem::path &) -> AssetID;
  /// @return `true` if the entry was found and removed, `false` if it didn't exist
  auto Unregister(const AssetID) -> bool;
  /// @brief Remove a list of entries
  auto Unregister(std::span<const AssetID>) -> void;
  /// @brief Remove all entries and associated metadata
  auto Reset() -> void;
  auto GetPath(const AssetID) const -> std::filesystem::path;
  auto GetStatus(const AssetID) const -> AssetStatus;
  auto GetType(const AssetID) const -> AssetType;
  auto GetName(const AssetID) const -> std::string;
  auto SetStatus(const AssetID, const AssetStatus) -> bool;
  /// @brief Execute a function for each asset of the specified type
  void ForEach(auto &&);
  static AssetType DeduceType(const std::filesystem::path &);
private:
  std::unordered_map<AssetID, AssetMetadata> idToMetadata;
  std::unordered_map<std::filesystem::path, AssetID> pathToId;
};
auto AssetDatabase::ForEach(auto &&func) -> void {
  for (auto &[id, metadata] : idToMetadata)
    func(id, metadata);
}
} // namespace kuki
