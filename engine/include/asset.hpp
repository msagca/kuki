#pragma once
#include <asset_type.hpp>
#include <concepts.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <string>
#include <typeindex>
#include <utility>
namespace kuki {
class KUKI_ENGINE_API Asset {
public:
  virtual ~Asset() = default;
  const AssetID id;
  EntityID previewId{};
  EntityID resourceId{};
  static auto GetMask(const AssetType) -> AssetMask;
  static auto GetType(const std::type_index) -> AssetType;
  static auto GetTypeIndex(const AssetType) -> std::type_index;
  static auto GetTypeName(const AssetType) -> std::string;
  static auto ForEachType(auto &&) -> void;
  auto GetName() const -> const std::string &;
  auto GetType() const -> AssetType;
  auto GetTypeIndex() const -> std::type_index;
  auto GetTypeName() const -> std::string;
  auto Rename(std::string = "") -> void;
  template <IsAsset T>
  auto As(this auto &self) -> ConstCorrectPointer<decltype(self), T>;
  template <IsAsset T>
  auto Is() const -> bool;
protected:
  template <typename T>
  explicit Asset(std::in_place_type_t<T>, AssetID = AssetID::Invalid, std::string = "");
  std::string name;
private:
  std::type_index typeIndex;
  static const std::unordered_map<AssetType, std::string> typeToName;
  static const std::unordered_map<AssetType, std::type_index> typeToTypeIndex;
  static const std::unordered_map<std::type_index, AssetType> typeIndexToType;
};
template <typename T>
Asset::Asset(std::in_place_type_t<T>, AssetID id, std::string name)
  : typeIndex(typeid(T)), id(id), name(std::move(name)) {}
auto Asset::ForEachType(auto &&func) -> void {
  for (const auto &[type, name] : typeToName)
    func(type, name);
}
template <IsAsset T>
auto Asset::As(this auto &self) -> ConstCorrectPointer<decltype(self), T> {
  if (self.template Is<T>())
    return static_cast<ConstCorrectPointer<decltype(self), T>>(&self);
  return nullptr;
}
template <IsAsset T>
auto Asset::Is() const -> bool {
  return typeIndex == typeid(T);
}
} // namespace kuki
