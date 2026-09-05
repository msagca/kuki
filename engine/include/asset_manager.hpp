#pragma once
#include <asset.hpp>
#include <bounding_box.hpp>
#include <color.hpp>
#include <concepts.hpp>
#include <entity_manager.hpp>
#include <filesystem>
#include <future>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <launch_path.hpp>
#include <manager.hpp>
#include <material_asset.hpp>
#include <memory>
#include <mesh_asset.hpp>
#include <model_asset.hpp>
#include <queue>
#include <scene.hpp>
#include <shader_asset.hpp>
#include <skybox_handle.hpp>
#include <string>
#include <string_view>
#include <texture_asset.hpp>
#include <texture_content.hpp>
#include <texture_handle.hpp>
#include <unordered_set>
#include <utility>
namespace kuki {
class KUKI_ENGINE_API AssetManager final : public Manager {
public:
  AssetManager(Application &);
  /// @brief Not copyable: it owns what it holds through `unique_ptr`.
  ///
  /// Explicit rather than left implicit because exporting a class from a shared library
  /// instantiates its implicit members too, and the implicit copy of a container of `unique_ptr`
  /// does not compile. Nothing copies this, so the declaration costs nothing and says so.
  AssetManager(const AssetManager &) = delete;
  auto operator=(const AssetManager &) -> AssetManager & = delete;
  auto Add(std::unique_ptr<Asset>, std::string = "") -> bool;
  auto CreatePrefab(const AssetID) -> EntityID;
  auto ForEach(this auto &, auto &&) -> void;
  auto ForEach(this auto &, const AssetType, auto &&) -> void;
  auto ForEachType(this auto &, auto &&) -> void;
  auto Get(this auto &, const std::string &) -> decltype(auto);
  auto Get(this auto &, const AssetID) -> decltype(auto);
  auto GetName(const AssetID) const -> std::string;
  auto GetPath(const AssetID) const -> std::filesystem::path;
  auto GetPrefabID(const AssetID) const -> EntityID;
  auto GetType(const AssetID) const -> AssetType;
  auto Instantiate(const AssetID, Scene &) -> EntityID;
  auto Instantiate(const std::string &, Scene &) -> EntityID;
  auto IsLoaded(const AssetID) const -> bool;
  auto ResolveModelInstance(Scene &, const EntityID, const ModelAsset &, const AssetID) -> void;
  auto LoadComputeFromSource(const std::string_view, std::string = "") -> AssetID;
  auto LoadShader(const std::filesystem::path &, const std::filesystem::path &, std::string = "", const MaterialType = MaterialType::Unlit) -> AssetID;
  auto LoadShaderFromSource(const std::string_view, const std::string_view, std::string = "", const MaterialType = MaterialType::Unlit) -> AssetID;
  auto Rename(const AssetID, std::string = "") -> bool;
  auto Update() -> void;
  template <IsAsset... T>
  auto ForEach(this auto &, auto &&) -> void;
  template <IsAsset T>
  auto Get(this auto &, const AssetID) -> decltype(auto);
  template <IsAsset T>
  auto Get(this auto &, const std::string &) -> decltype(auto);
  template <typename... T>
  auto GetComponent(this auto &, const AssetID) -> decltype(auto);
  template <typename... T>
  auto GetComponent(this auto &, const EntityID) -> decltype(auto);
  template <IsAsset T>
  auto Load(const std::filesystem::path &, std::string = "", const AssetID = {}) -> AssetID;
  template <IsAsset T>
  auto LoadAsync(const std::filesystem::path &, std::string = "", const AssetID = {}) -> AssetID;
private:
  EntityManager prefabManager;
  std::queue<std::unique_ptr<Asset>> loadedAssets;
  std::unordered_map<AssetID, EntityID> idToPrefabId;
  std::unordered_map<AssetID, std::future<std::unique_ptr<Asset>>> idToFuture;
  std::unordered_map<AssetID, std::unique_ptr<Asset>> idToAsset;
  std::unordered_map<AssetID, std::string> idToName;
  std::unordered_map<AssetID, std::filesystem::path> idToPath;
  std::unordered_map<std::filesystem::path, AssetID> pathToId;
  std::unordered_multimap<std::string, AssetID> nameToId;
  std::unordered_map<std::type_index, std::unordered_set<AssetID>> typeIndexToAssetSet;
  static const std::unordered_map<std::type_index, AssetType> typeIndexToAssetType;
  template <IsAsset T>
  static auto GetAssetType() -> AssetType;
  auto CreateNodePrefab(const AssetID, const ModelAsset &, const int = 0) -> EntityID;
  auto Load(std::unique_ptr<Asset>) -> void;
  auto RegisterModelTextures(const ModelAsset &) -> void;
  auto ResolveNodeReferences(ModelAsset &) -> void;
  auto SetName(const AssetID, std::string) -> void;
  template <IsAsset T>
  auto CreatePrefab(const AssetID) -> EntityID;
  template <IsAsset T>
  auto Load(const AssetID, const std::filesystem::path &) -> std::unique_ptr<Asset>;
  template <IsAsset T>
  auto Register(const std::filesystem::path &, const std::string &, const AssetID = {}) -> AssetID;
};
auto AssetManager::ForEach(this auto &self, auto &&func) -> void {
  for (auto &[id, asset] : self.idToAsset)
    func(asset.get());
}
auto AssetManager::ForEach(this auto &self, const AssetType type, auto &&func) -> void {
  const auto typeIndex = Asset::GetTypeIndex(type);
  if (auto it = self.typeIndexToAssetSet.find(typeIndex); it != self.typeIndexToAssetSet.end())
    for (const auto &id : it->second)
      if (auto it2 = self.idToAsset.find(id); it2 != self.idToAsset.end()) {
        auto asset = it2->second.get();
        if (type == AssetType::Shader)
          if (auto shaderAsset = asset->template As<ShaderAsset>(); shaderAsset && shaderAsset->shaderType == ShaderType::Vertex)
            continue;
        func(asset);
      }
}
auto AssetManager::ForEachType(this auto &self, auto &&func) -> void {
  for (const auto &[typeIndex, _] : self.typeIndexToAssetSet) {
    const auto type = Asset::GetType(typeIndex);
    const auto name = Asset::GetTypeName(type);
    func(type, name);
  }
}
auto AssetManager::Get(this auto &self, const AssetID id) -> decltype(auto) {
  if (auto it = self.idToFuture.find(id); it != self.idToFuture.end()) {
    auto asset = it->second.get();
    self.Load(std::move(asset));
    self.idToFuture.erase(it);
  }
  if (auto it = self.idToAsset.find(id); it != self.idToAsset.end())
    return it->second.get();
  return ConstCorrectPointer<decltype(self), Asset>(nullptr);
}
auto AssetManager::Get(this auto &self, const std::string &name) -> decltype(auto) {
  if (auto ids = self.nameToId.equal_range(name); ids.first != ids.second)
    return self.Get(ids.first->second);
  return ConstCorrectPointer<decltype(self), Asset>(nullptr);
}
template <IsAsset T>
auto AssetManager::GetAssetType() -> AssetType {
  const auto typeIndex = std::type_index(typeid(T));
  if (auto it = typeIndexToAssetType.find(typeIndex); it != typeIndexToAssetType.end())
    return it->second;
  return AssetType::Texture;
}
template <IsAsset... T>
auto AssetManager::ForEach(this auto &self, auto &&func) -> void {
  static_assert(sizeof...(T) > 0, "`ForEach` requires at least one type parameter.");
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    const auto typeIndex = std::type_index(typeid(C));
    if (auto it = self.typeIndexToAssetSet.find(typeIndex); it != self.typeIndexToAssetSet.end())
      for (const auto &id : it->second)
        if (auto it2 = self.idToAsset.find(id); it2 != self.idToAsset.end())
          func(it2->second.get());
  } else
    (self.template ForEach<T>(std::forward<decltype(func)>(func)), ...);
}
template <IsAsset T>
auto AssetManager::Get(this auto &self, const AssetID id) -> decltype(auto) {
  if (auto it = self.idToFuture.find(id); it != self.idToFuture.end()) {
    auto asset = it->second.get();
    self.idToFuture.erase(it);
    self.Load(std::move(asset));
  }
  if (auto it = self.idToAsset.find(id); it != self.idToAsset.end()) {
    auto asset = it->second.get();
    return asset->template As<T>();
  }
  return ConstCorrectPointer<decltype(self), T>(nullptr);
}
template <IsAsset T>
auto AssetManager::Get(this auto &self, const std::string &name) -> decltype(auto) {
  if (auto ids = self.nameToId.equal_range(name); ids.first != ids.second) {
    const auto typeIndex = std::type_index(typeid(T));
    if (auto it = self.typeIndexToAssetSet.find(typeIndex); it != self.typeIndexToAssetSet.end())
      for (const auto &id : it->second)
        if (id == ids.first->second)
          return self.template Get<T>(id);
  }
  return ConstCorrectPointer<decltype(self), T>(nullptr);
}
template <typename... T>
auto AssetManager::GetComponent(this auto &self, const AssetID assetId) -> decltype(auto) {
  const auto prefabId = self.GetPrefabID(assetId);
  return self.prefabManager.template GetComponent<T...>(prefabId);
}
template <typename... T>
auto AssetManager::GetComponent(this auto &self, const EntityID id) -> decltype(auto) {
  return self.prefabManager.template GetComponent<T...>(id);
}
template <IsAsset T>
auto AssetManager::Load(const std::filesystem::path &path, std::string name, const AssetID forcedId) -> AssetID {
  if (name.empty())
    name = path.stem().string();
  const auto id = Register<T>(path, name, forcedId);
  if (idToAsset.contains(id))
    return id;
  auto asset = Load<T>(id, ResolvePath(path));
  Load(std::move(asset));
  return id;
}
template <IsAsset T>
auto AssetManager::LoadAsync(const std::filesystem::path &path, std::string name, const AssetID forcedId) -> AssetID {
  if (name.empty())
    name = path.stem().string();
  const auto id = Register<T>(path, name, forcedId);
  if (auto it = idToAsset.find(id); it != idToAsset.end())
    return id;
  // FIXME: `idToAsset` is not updated until the future is completed
  if (auto it = idToFuture.find(id); it == idToFuture.end()) {
    auto future = std::async(std::launch::async, [id, resolved = ResolvePath(path), this]() {
      return Load<T>(id, resolved);
    });
    if (future.valid())
      idToFuture.emplace(id, std::move(future));
  }
  return id;
}
template <IsAsset T>
auto AssetManager::CreatePrefab(const AssetID) -> EntityID {
  return EntityID::Invalid;
}
template <>
inline auto AssetManager::CreatePrefab<MaterialAsset>(const AssetID assetId) -> EntityID {
  if (auto materialAsset = Get<MaterialAsset>(assetId); materialAsset) {
    const auto name = GetName(assetId);
    const auto prefabId = prefabManager.Create(name);
    auto materialHandle = prefabManager.AddComponent<MaterialHandle>(prefabId);
    materialHandle->assetId = assetId;
    return prefabId;
  }
  return EntityID::Invalid;
}
template <>
inline auto AssetManager::CreatePrefab<MeshAsset>(const AssetID assetId) -> EntityID {
  if (auto meshAsset = Get<MeshAsset>(assetId); meshAsset) {
    const auto name = GetName(assetId);
    const auto prefabId = prefabManager.Create(name);
    prefabManager.AddComponent<Transform>(prefabId);
    auto bounds = prefabManager.AddComponent<BoundingBox>(prefabId);
    *bounds = meshAsset->bounds;
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
inline auto AssetManager::CreatePrefab<ModelAsset>(const AssetID assetId) -> EntityID {
  if (auto modelAsset = Get<ModelAsset>(assetId); modelAsset)
    return CreateNodePrefab(assetId, *modelAsset);
  return EntityID::Invalid;
}
template <>
inline auto AssetManager::CreatePrefab<TextureAsset>(const AssetID assetId) -> EntityID {
  if (auto textureAsset = Get<TextureAsset>(assetId); textureAsset) {
    const auto name = GetName(assetId);
    const auto prefabId = prefabManager.Create(name);
    auto textureHandle = prefabManager.AddComponent<TextureHandle>(prefabId);
    textureHandle->assetId = assetId;
    return prefabId;
  }
  return EntityID::Invalid;
}
template <IsAsset T>
auto AssetManager::Load(const AssetID, const std::filesystem::path &) -> std::unique_ptr<Asset> {
  return nullptr;
}
template <>
auto KUKI_ENGINE_API AssetManager::Load<ModelAsset>(const AssetID id, const std::filesystem::path &path) -> std::unique_ptr<Asset>;
template <>
auto KUKI_ENGINE_API AssetManager::Load<ShaderAsset>(const AssetID id, const std::filesystem::path &path) -> std::unique_ptr<Asset>;
/// @brief Reads a material from a small JSON description of its untextured surface values.
///
/// Materials otherwise only ever arrive inside a model file, which leaves no way to author one for
/// a scene that is built from primitives. Every field is optional and falls back to the value
/// `MaterialFallback` already declares, so a description only states what it changes.
template <>
auto KUKI_ENGINE_API AssetManager::Load<MaterialAsset>(const AssetID id, const std::filesystem::path &path) -> std::unique_ptr<Asset>;
template <>
auto KUKI_ENGINE_API AssetManager::Load<TextureAsset>(const AssetID id, const std::filesystem::path &path) -> std::unique_ptr<Asset>;
template <IsAsset T>
auto AssetManager::Register(const std::filesystem::path &path, const std::string &name, const AssetID forcedId) -> AssetID {
  if (path.empty())
    return {};
  if (auto it = pathToId.find(path); it != pathToId.end())
    return it->second;
  const auto typeIndex = std::type_index(typeid(T));
  const auto id = forcedId ? forcedId : AssetID::Generate();
  pathToId.emplace(path, id);
  idToPath.emplace(id, path);
  SetName(id, name);
  typeIndexToAssetSet[typeIndex].insert(id);
  return id;
}
} // namespace kuki
