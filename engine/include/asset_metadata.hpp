#pragma once
#include <asset_type.hpp>
#include <filesystem>
#include <id.hpp>
#include <string>
namespace kuki {
enum class AssetStatus : uint8_t {
  Loaded,
  Missing,
  Modified,
  Registered,
  Unregistered
};
struct AssetMetadata {
  AssetID id{};
  std::string name{};
  std::filesystem::path path{};
  AssetType type{AssetType::Texture};
  AssetStatus status{AssetStatus::Unregistered};
};
} // namespace kuki
