#pragma once
#include <asset.hpp>
#include <asset_database.hpp>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <bounding_box.hpp>
#include <concepts.hpp>
#include <entity_manager.hpp>
#include <filesystem>
#include <fstream>
#include <future>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <material_asset.hpp>
#include <memory>
#include <mesh_asset.hpp>
#include <queue>
#include <scene.hpp>
#include <scene_asset.hpp>
#include <shader_asset.hpp>
#include <skybox_asset.hpp>
#include <skybox_handle.hpp>
#include <stb_image.h>
#include <texture_asset.hpp>
#include <texture_content.hpp>
#include <texture_handle.hpp>
#include <tinyexr.h>
#include <unordered_set>
#include <utility>
namespace kuki {
class KUKI_ENGINE_API AssetManager {
public:
  auto CreatePrefab(const AssetID) -> EntityID;
  auto Get(this auto &self, const AssetID) -> ConstCorrectPointer<decltype(self), Asset>;
  auto Get(this auto &, const std::string &) -> decltype(auto);
  auto GetID(const std::string &) const -> AssetID;
  auto GetName(const AssetID) const -> std::string;
  auto GetPath(const AssetID) const -> std::filesystem::path;
  auto GetPrefabID(const AssetID) const -> EntityID;
  auto Instantiate(const AssetID, Scene &) -> EntityID;
  auto Instantiate(const std::string &, Scene &) -> EntityID;
  auto IsRegistered(const AssetID) const -> bool;
  auto Unload(const AssetID) -> bool;
  auto Update() -> void;
  auto ForEach(this auto &, const AssetType, auto &&) -> void;
  auto ForEachPerType(this auto &, auto &&) -> void;
  auto ForEachPrefab(this auto &, auto &&) -> void;
  auto ForEachType(this auto &, auto &&) -> void;
  /// @brief Add an existing asset in memory
  template <IsAsset T>
  auto Add(std::unique_ptr<T>) -> bool;
  template <IsAsset... T>
  auto ForEach(this auto &, auto &&) -> void;
  template <IsAsset T>
  auto Get(this auto &self, const AssetID) -> ConstCorrectPointer<decltype(self), T>;
  template <IsAsset T>
  auto Load(const std::filesystem::path &, std::string = "") -> AssetID;
  template <IsAsset T>
  auto LoadAsync(const std::filesystem::path &, std::string = "") -> AssetID;
private:
  AssetDatabase assetDb;
  EntityManager prefabManager;
  std::queue<std::unique_ptr<Asset>> loadedAssets;
  std::unordered_map<AssetID, EntityID> idToPrefabId;
  std::unordered_map<AssetID, std::future<std::unique_ptr<Asset>>> idToFuture;
  std::unordered_map<AssetID, std::unique_ptr<Asset>> idToAsset;
  std::unordered_map<std::type_index, std::unordered_set<AssetID>> typeIndexToAssetSet;
  static auto AssimpToGlmMat4(const aiMatrix4x4 &) -> glm::mat4;
  static auto AssimpTexToContent(const aiTextureType) -> TextureContent;
  auto CreateNodePrefab(const AssetID, const SceneAsset &, const int = 0, const EntityID = EntityID::Invalid) -> EntityID;
  auto Load(std::unique_ptr<Asset>) -> void;
  auto LoadMaterial(const aiMaterial &, SceneAsset &, const std::filesystem::path & = {}) -> unsigned int;
  auto LoadMesh(const aiMesh &, SceneAsset &, BoundingBox &, unsigned int) -> unsigned int;
  auto LoadNode(const aiNode &, const aiScene &, SceneAsset &, const std::filesystem::path & = {}, int = -1) -> unsigned int;
  auto LoadTexture(const aiMaterial &, const aiTextureType, SceneMaterial &, SceneAsset &, const std::filesystem::path &) -> void;
  auto Unregister(const AssetID) -> bool;
  template <IsAsset T>
  auto CreatePrefab(const AssetID) -> EntityID;
  template <IsAsset T>
  auto Load(const AssetID, std::string = "") -> std::unique_ptr<Asset>;
  template <IsAsset T>
  auto Load(const AssetID, const std::filesystem::path &, std::string = "") -> std::unique_ptr<Asset>;
  template <IsAsset T>
  auto LoadAsync(const AssetID, std::string = "") -> std::future<std::unique_ptr<Asset>>;
  template <IsAsset T>
  auto Register(const std::filesystem::path &, std::string = "") -> AssetID;
};
auto AssetManager::Get(this auto &self, const AssetID id) -> ConstCorrectPointer<decltype(self), Asset> {
  if (auto it = self.idToFuture.find(id); it != self.idToFuture.end()) {
    auto asset = it->second.get();
    self.Load(std::move(asset));
    self.idToFuture.erase(it);
  }
  if (auto it = self.idToAsset.find(id); it != self.idToAsset.end())
    return it->second.get();
  return nullptr;
}
auto AssetManager::Get(this auto &self, const std::string &name) -> decltype(auto) {
  const auto &id = self.assetDb.GetID(name);
  return self.Get(id);
}
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
template <IsAsset T>
auto AssetManager::Add(std::unique_ptr<T> asset) -> bool {
  if (auto it = idToAsset.find(asset->id); it != idToAsset.end())
    return false;
  if (!assetDb.Register(*asset))
    return false;
  typeIndexToAssetSet[asset->GetTypeIndex()].insert(asset->id);
  idToAsset.emplace(asset->id, std::move(asset));
  return true;
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
auto AssetManager::Load(const std::filesystem::path &path, std::string name) -> AssetID {
  const auto id = Register<T>(path, name);
  Load<T>(id, std::move(name));
  return id;
}
template <IsAsset T>
auto AssetManager::LoadAsync(const std::filesystem::path &path, std::string name) -> AssetID {
  const auto id = Register<T>(path, name);
  LoadAsync<T>(id, std::move(name));
  return id;
}
template <IsAsset T>
auto AssetManager::CreatePrefab(const AssetID) -> EntityID {
  return EntityID::Invalid;
}
template <IsAsset T>
auto AssetManager::Load(const AssetID id, std::string name) -> std::unique_ptr<Asset> {
  const auto status = assetDb.GetStatus(id);
  if (status == AssetStatus::Unregistered || status == AssetStatus::Missing)
    return nullptr;
  if (status != AssetStatus::Loaded) {
    auto path = GetPath(id);
    if (path.empty())
      return nullptr;
    auto asset = Load<T>(id, path, std::move(name));
    Load(std::move(asset));
  }
  return nullptr;
}
template <IsAsset T>
auto AssetManager::LoadAsync(const AssetID id, std::string name) -> std::future<std::unique_ptr<Asset>> {
  const auto status = assetDb.GetStatus(id);
  if (status == AssetStatus::Unregistered || status == AssetStatus::Missing)
    return {};
  if (status != AssetStatus::Loaded)
    if (auto it = idToFuture.find(id); it == idToFuture.end()) {
      auto path = GetPath(id);
      if (path.empty())
        return {};
      auto future = std::async(std::launch::async, [=, this]() {
        return Load<T>(id, path, std::move(name));
      });
      if (future.valid())
        idToFuture.emplace(id, std::move(future));
    }
  return {};
}
template <IsAsset T>
auto AssetManager::Register(const std::filesystem::path &path, std::string name) -> AssetID {
  return assetDb.Register<T>(path, std::move(name));
}
template <>
inline auto AssetManager::CreatePrefab<MaterialAsset>(const AssetID assetId) -> EntityID {
  if (auto materialAsset = Get<MaterialAsset>(assetId); materialAsset) {
    const auto &name = GetName(assetId);
    const auto &prefabId = prefabManager.Create(name);
    auto materialHandle = prefabManager.AddComponent<MaterialHandle>(prefabId);
    materialHandle->assetId = assetId;
    return prefabId;
  }
  return EntityID::Invalid;
}
template <>
inline auto AssetManager::CreatePrefab<MeshAsset>(const AssetID assetId) -> EntityID {
  if (auto meshAsset = Get<MeshAsset>(assetId); meshAsset) {
    const auto &name = GetName(assetId);
    const auto &prefabId = prefabManager.Create(name);
    prefabManager.AddComponent<Transform>(prefabId);
    auto meshHandle = prefabManager.AddComponent<MeshHandle>(prefabId);
    meshHandle->assetId = assetId;
    auto materialHandle = prefabManager.AddComponent<MaterialHandle>(prefabId);
    materialHandle->assetId = meshAsset->material;
    CreatePrefab(materialHandle->assetId);
    return prefabId;
  }
  return EntityID::Invalid;
}
template <>
inline auto AssetManager::CreatePrefab<SceneAsset>(const AssetID assetId) -> EntityID {
  if (auto sceneAsset = Get<SceneAsset>(assetId); sceneAsset)
    return CreateNodePrefab(assetId, *sceneAsset);
  return EntityID::Invalid;
}
template <>
inline auto AssetManager::CreatePrefab<SkyboxAsset>(const AssetID assetId) -> EntityID {
  if (auto skyboxAsset = Get<SkyboxAsset>(assetId); skyboxAsset) {
    const auto &name = GetName(assetId);
    const auto &prefabId = prefabManager.Create(name);
    auto skyboxHandle = prefabManager.AddComponent<SkyboxHandle>(prefabId);
    skyboxHandle->assetId = assetId;
    return prefabId;
  }
  return EntityID::Invalid;
}
template <>
inline auto AssetManager::CreatePrefab<TextureAsset>(const AssetID assetId) -> EntityID {
  if (auto textureAsset = Get<TextureAsset>(assetId); textureAsset) {
    const auto &name = GetName(assetId);
    const auto &prefabId = prefabManager.Create(name);
    auto textureHandle = prefabManager.AddComponent<TextureHandle>(prefabId);
    textureHandle->assetId = assetId;
    return prefabId;
  }
  return EntityID::Invalid;
}
template <IsAsset T>
auto AssetManager::Load(const AssetID, const std::filesystem::path &, std::string) -> std::unique_ptr<Asset> {
  return nullptr;
}
template <>
inline auto AssetManager::Load<SceneAsset>(const AssetID id, const std::filesystem::path &path, std::string name) -> std::unique_ptr<Asset> {
  Assimp::Importer importer;
  const auto aiScene = importer.ReadFile(path.string(), aiProcess_CalcTangentSpace | aiProcess_GlobalScale | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType | aiProcess_Triangulate);
  if (!aiScene) {
    spdlog::error("Assimp: {}", importer.GetErrorString());
    return nullptr;
  }
  if (!aiScene->mRootNode)
    return nullptr;
  auto scene = std::make_unique<SceneAsset>(id, std::move(name));
  LoadNode(*aiScene->mRootNode, *aiScene, *scene.get());
  spdlog::info("Loaded scene asset: {}", path.string());
  return scene;
}
template <>
inline auto AssetManager::Load<ShaderAsset>(const AssetID id, const std::filesystem::path &path, std::string name) -> std::unique_ptr<Asset> {
  auto shader = std::make_unique<ShaderAsset>(id, std::move(name));
  std::ifstream fs(path);
  if (!fs) {
    spdlog::error("Failed to open shader file: {}", path.string());
    return shader;
  }
  std::stringstream ss;
  ss << fs.rdbuf();
  if (fs.fail()) {
    spdlog::error("Failed to read shader file: {}", path.string());
    return shader;
  }
  fs.close();
  shader->text = ss.str();
  spdlog::info("Loaded shader asset: {}", path.string());
  return shader;
}
template <>
inline auto AssetManager::Load<SkyboxAsset>(const AssetID id, const std::filesystem::path &path, std::string name) -> std::unique_ptr<Asset> {
  auto skybox = std::make_unique<SkyboxAsset>(id, std::move(name));
  const auto ext = path.extension().string();
  if (ext == ".exr") {
    float *data = nullptr;
    const char *errMsg = nullptr;
    auto result = LoadEXR(&data, &skybox->width, &skybox->height, path.string().c_str(), &errMsg);
    if (result != TINYEXR_SUCCESS) {
      if (errMsg) {
        spdlog::error("TinyEXR: {}", errMsg);
        FreeEXRErrorMessage(errMsg);
      } else
        spdlog::error("TinyEXR failed to load: {}", path.string());
    } else if (data) {
      skybox->channels = 4;
      const auto size = skybox->width * skybox->height * skybox->channels;
      skybox->data.assign(data, data + size);
      free(data);
      spdlog::info("Loaded skybox asset: {}", path.string());
    }
  } else if (auto data = stbi_loadf(path.string().c_str(), &skybox->width, &skybox->height, &skybox->channels, 0); data) {
    const auto size = skybox->width * skybox->height * skybox->channels;
    skybox->data.assign(data, data + size);
    stbi_image_free(data);
    spdlog::info("Loaded skybox asset: {}", path.string());
  }
  return skybox;
}
} // namespace kuki
