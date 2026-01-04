#pragma once
#include <asset.hpp>
#include <asset_database.hpp>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <compute_type.hpp>
#include <concepts.hpp>
#include <entity_manager.hpp>
#include <filesystem>
#include <fstream>
#include <future>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <memory>
#include <queue>
#include <scene.hpp>
#include <scene_asset.hpp>
#include <shader_asset.hpp>
#include <skybox_asset.hpp>
#include <skybox_handle.hpp>
#include <stb_image.h>
#include <texture_asset.hpp>
#include <texture_content.hpp>
#include <tinyexr.h>
#include <unordered_set>
#include <utility>
namespace kuki {
class KUKI_ENGINE_API AssetManager {
public:
  auto Get(this auto &self, const AssetID) -> ConstCorrectPointer<decltype(self), Asset>;
  auto GetName(const AssetID) const -> std::string;
  auto GetPath(const AssetID) const -> std::filesystem::path;
  auto Instantiate(const AssetID, Scene *) -> EntityID;
  auto Load(const std::filesystem::path &) -> AssetID;
  auto LoadAsync(const std::filesystem::path &) -> AssetID;
  auto Unload(const AssetID) -> bool;
  auto Update() -> void;
  // templates
  auto ForEach(this auto &, const AssetType, auto &&) -> void;
  auto ForEachPerType(this auto &, auto &&) -> void;
  auto ForEachPrefab(this auto &, auto &&) -> void;
  auto ForEachType(this auto &, auto &&) -> void;
  template <IsAsset... T>
  auto ForEach(this auto &, auto &&) -> void;
  template <IsAsset T>
  auto Get(this auto &self, const AssetID) -> ConstCorrectPointer<decltype(self), T>;
private:
  AssetDatabase assetDb;
  EntityManager prefabManager;
  std::queue<std::unique_ptr<Asset>> loadedAssets;
  std::unordered_map<AssetID, EntityID> idToPrefabId;
  std::unordered_map<AssetID, std::future<std::unique_ptr<Asset>>> idToFuture;
  std::unordered_map<AssetID, std::unique_ptr<Asset>> idToAsset;
  std::unordered_map<std::type_index, std::unordered_set<AssetID>> typeIndexToAssetSet;
  auto AssimpToGlmMat4(const aiMatrix4x4 &) -> glm::mat4;
  auto AssimpTexToContent(const aiTextureType) -> TextureContent;
  auto CreateNodePrefab(const AssetID, const SceneAsset &, const int = 0, const EntityID = EntityID::Invalid) -> EntityID;
  auto CreatePrefab(const AssetID) -> EntityID;
  auto Load(const AssetID) -> bool;
  auto Load(std::unique_ptr<Asset>) -> void;
  auto LoadAsync(const AssetID) -> bool;
  auto LoadAsyncBase(const AssetID) -> std::future<std::unique_ptr<Asset>>;
  auto LoadBase(const AssetID) -> std::unique_ptr<Asset>;
  auto LoadMaterial(const aiMaterial &, SceneAsset &, const std::filesystem::path & = {}) -> int;
  auto LoadMesh(const aiMesh &, SceneAsset &) -> int;
  auto LoadNode(const aiNode &, const aiScene &, SceneAsset &, const std::filesystem::path & = {}, int = -1) -> int;
  auto LoadTexture(const aiMaterial &, const aiTextureType, MaterialAsset &, SceneAsset &, const std::filesystem::path &) -> void;
  auto Register(const std::filesystem::path &) -> AssetID;
  auto Unregister(const AssetID) -> bool;
  // templates
  template <IsAsset T>
  auto CreatePrefab(const AssetID) -> EntityID;
  template <IsAsset T>
  auto Load(const AssetID) -> std::unique_ptr<Asset>;
  template <IsAsset T>
  auto Load(const AssetID, const std::filesystem::path &) -> std::unique_ptr<Asset>;
  template <IsAsset T>
  auto LoadAsync(const AssetID) -> std::future<std::unique_ptr<Asset>>;
};
auto AssetManager::ForEach(this auto &self, const AssetType type, auto &&func) -> void {
  const auto typeIndex = Asset::GetTypeIndex(type);
  if (auto it = self.typeIndexToAssetSet.find(typeIndex); it != self.typeIndexToAssetSet.end())
    for (const auto &id : it->second) {
      const auto name = self.GetName(id);
      func(id, name);
    }
}
auto AssetManager::ForEachPerType(this auto &self, auto &&func) -> void {
  for (const auto &[typeIndex, assetIDs] : self.typeIndexToAssetSet) {
    const auto type = Asset::GetType(typeIndex);
    for (const auto &id : assetIDs) {
      const auto name = self.GetName(id);
      func(type, id, name);
    }
  }
}
auto AssetManager::ForEachPrefab(this auto &self, auto &&func) -> void {
  self.prefabManager.ForEach(std::forward<decltype(func)>(func));
}
auto AssetManager::ForEachType(this auto &self, auto &&func) -> void {
  for (const auto &[typeIndex, _] : self.typeIndexToAssetSet) {
    const auto type = Asset::GetType(typeIndex);
    const auto name = Asset::GetTypeName(type);
    func(type, name);
  }
}
template <IsAsset... T>
auto AssetManager::ForEach(this auto &self, auto &&func) -> void {
  static_assert(sizeof...(T) > 0, "`ForEach` requires at least one type parameter.");
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    const auto typeIndex = std::type_index(typeid(C));
    if (auto it = self.typeIndexToAssetSet.find(typeIndex); it != self.typeIndexToAssetSet.end())
      for (const auto &id : it->second) {
        const auto name = self.GetName(id);
        func(id, name);
      }
  } else
    (self.template ForEach<T>(std::forward<decltype(func)>(func)), ...);
}
template <IsAsset T>
auto AssetManager::Get(this auto &self, const AssetID id) -> ConstCorrectPointer<decltype(self), T> {
  if (auto it = self.idToFuture.find(id); it != self.idToFuture.end()) {
    auto asset = it->second.get();
    self.idToFuture.erase(it);
    self.Load(std::move(asset));
  }
  if (auto it = self.idToAsset.find(id); it != self.idToAsset.end()) {
    auto asset = it->second.get();
    if (asset->template Is<T>())
      return asset->template As<T>();
  }
  return nullptr;
}
template <IsAsset T>
auto AssetManager::CreatePrefab(const AssetID) -> EntityID {
  return {};
}
template <IsAsset T>
auto AssetManager::Load(const AssetID id) -> std::unique_ptr<Asset> {
  auto path = GetPath(id);
  if (path.empty())
    return nullptr;
  return Load<T>(id, path);
}
template <IsAsset T>
auto AssetManager::Load(const AssetID id, const std::filesystem::path &path) -> std::unique_ptr<Asset> {
  return nullptr;
}
template <IsAsset T>
auto AssetManager::LoadAsync(const AssetID id) -> std::future<std::unique_ptr<Asset>> {
  auto path = GetPath(id);
  if (path.empty())
    return {};
  return std::async(std::launch::async, [this, id, path]() {
    return Load<T>(id, path);
  });
}
#include <asset_manager.inl>
} // namespace kuki
