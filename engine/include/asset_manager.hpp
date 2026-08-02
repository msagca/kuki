#pragma once
#include <asset.hpp>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <bounding_box.hpp>
#include <color.hpp>
#include <concepts.hpp>
#include <entity_manager.hpp>
#include <filesystem>
#include <fstream>
#include <future>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <manager.hpp>
#include <material_asset.hpp>
#include <memory>
#include <mesh_asset.hpp>
#include <model_asset.hpp>
#include <queue>
#include <scene.hpp>
#include <shader_asset.hpp>
#include <skybox_handle.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <string>
#include <string_view>
#include <texture_asset.hpp>
#include <texture_content.hpp>
#include <texture_handle.hpp>
#include <tinyexr.h>
#include <unordered_set>
#include <utility>
namespace kuki {
class KUKI_ENGINE_API AssetManager final : public Manager {
public:
  AssetManager(Application &);
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
  static auto AssimpToGlmMat4(const aiMatrix4x4 &) -> glm::mat4;
  static auto AssimpTexToContent(const aiTextureType) -> TextureContent;
  template <IsAsset T>
  static auto GetAssetType() -> AssetType;
  auto CreateNodePrefab(const AssetID, const ModelAsset &, const int = 0) -> EntityID;
  auto Load(std::unique_ptr<Asset>) -> void;
  auto LoadMaterial(std::unordered_map<std::string, unsigned int> &, const aiMaterial &, ModelAsset &, const aiScene &, const std::filesystem::path & = {}, const std::string & = {}) -> unsigned int;
  auto LoadMesh(const aiMesh &, ModelAsset &, BoundingBox &, unsigned int, const std::string & = {}) -> unsigned int;
  auto LoadNode(std::unordered_map<std::string, unsigned int> &, const aiNode &, const aiScene &, ModelAsset &, const std::filesystem::path & = {}, const std::string & = {}, int = -1) -> unsigned int;
  auto LoadTexture(std::unordered_map<std::string, unsigned int> &, const aiMaterial &, const aiTextureType, ModelMaterial &, ModelAsset &, const aiScene &, const std::filesystem::path &, const std::string & = {}) -> void;
  auto ParseAnimations(const aiScene &, ModelAsset &) -> void;
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
  auto asset = Load<T>(id, path);
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
    auto future = std::async(std::launch::async, [id, path, this]() {
      return Load<T>(id, path);
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
inline auto AssetManager::Load<ModelAsset>(const AssetID id, const std::filesystem::path &path) -> std::unique_ptr<Asset> {
  const auto pathNormStr = path.lexically_normal().string();
  Assimp::Importer importer;
  const auto aiScene = importer.ReadFile(pathNormStr, aiProcess_CalcTangentSpace | aiProcess_GlobalScale | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType | aiProcess_Triangulate);
  if (!aiScene) {
    spdlog::error("[AssetManager] {}", importer.GetErrorString());
    return nullptr;
  }
  if (!aiScene->mRootNode)
    return nullptr;
  auto model = std::make_unique<ModelAsset>(id);
  std::unordered_map<std::string, unsigned int> visited;
  LoadNode(visited, *aiScene->mRootNode, *aiScene, *model.get(), path.parent_path(), path.filename().string());
  ParseAnimations(*aiScene, *model.get());
  ResolveNodeReferences(*model.get());
  spdlog::info("[AssetManager] loaded model: {}", pathNormStr);
  return model;
}
template <>
inline auto AssetManager::Load<ShaderAsset>(const AssetID id, const std::filesystem::path &path) -> std::unique_ptr<Asset> {
  const auto pathNormStr = path.lexically_normal().string();
  auto shader = std::make_unique<ShaderAsset>(id);
  const auto ext = path.extension().string();
  if (ext == ".vert" || ext == ".vs")
    shader->shaderType = ShaderType::Vertex;
  else if (ext == ".frag" || ext == ".fs")
    shader->shaderType = ShaderType::Fragment;
  else if (ext == ".geom" || ext == ".gs")
    shader->shaderType = ShaderType::Geometry;
  else if (ext == ".comp")
    shader->shaderType = ShaderType::Compute;
  else {
    spdlog::warn("[AssetManager] unable to infer shader type for file: {}", pathNormStr);
    return shader;
  }
  std::ifstream fs(path);
  if (!fs) {
    spdlog::error("[AssetManager] failed to open shader file: {}", pathNormStr);
    return shader;
  }
  std::stringstream ss;
  ss << fs.rdbuf();
  if (fs.fail()) {
    spdlog::error("[AssetManager] failed to read shader file: {}", pathNormStr);
    return shader;
  }
  fs.close();
  shader->text = ss.str();
  spdlog::info("[AssetManager] loaded shader: {}", pathNormStr);
  return shader;
}
template <>
inline auto AssetManager::Load<TextureAsset>(const AssetID id, const std::filesystem::path &path) -> std::unique_ptr<Asset> {
  const auto pathNormStr = path.lexically_normal().string();
  auto textureAsset = std::make_unique<TextureAsset>(id);
  const auto ext = path.extension().string();
  if (ext == ".exr") {
    float *data = nullptr;
    const char *errMsg = nullptr;
    auto result = LoadEXR(&data, &textureAsset->texture.width, &textureAsset->texture.height, pathNormStr.c_str(), &errMsg);
    if (result != TINYEXR_SUCCESS) {
      if (errMsg) {
        spdlog::error("[AssetManager] {}", errMsg);
        FreeEXRErrorMessage(errMsg);
      } else
        spdlog::error("[AssetManager] failed to load texture: {}", pathNormStr);
    } else if (data) {
      textureAsset->texture.channels = 4;
      const auto size = textureAsset->texture.width * textureAsset->texture.height * textureAsset->texture.channels;
      textureAsset->texture.data = std::vector<float>();
      if (auto textureData = std::get_if<std::vector<float>>(&textureAsset->texture.data))
        textureData->assign(data, data + size);
      textureAsset->texture.range = ColorRange::HDR;
      textureAsset->texture.content = TextureContent::Skybox;
      textureAsset->texture.flipY = true;
      free(data);
      spdlog::info("[AssetManager] loaded texture: {}", pathNormStr);
    }
  } else if (ext == ".hdr") {
    if (auto data = stbi_loadf(path.string().c_str(), &textureAsset->texture.width, &textureAsset->texture.height, &textureAsset->texture.channels, 0); data) {
      const auto size = textureAsset->texture.width * textureAsset->texture.height * textureAsset->texture.channels;
      textureAsset->texture.data = std::vector<float>();
      if (auto textureData = std::get_if<std::vector<float>>(&textureAsset->texture.data))
        textureData->assign(data, data + size);
      textureAsset->texture.range = ColorRange::HDR;
      textureAsset->texture.content = TextureContent::Skybox;
      stbi_image_free(data);
      spdlog::info("[AssetManager] loaded texture: {}", pathNormStr);
    } else
      spdlog::error("[AssetManager] failed to load texture: {}", pathNormStr);
  } else {
    if (auto data = stbi_load(path.string().c_str(), &textureAsset->texture.width, &textureAsset->texture.height, &textureAsset->texture.channels, 0); data) {
      const auto size = textureAsset->texture.width * textureAsset->texture.height * textureAsset->texture.channels;
      textureAsset->texture.data = std::vector<unsigned char>();
      if (auto textureData = std::get_if<std::vector<unsigned char>>(&textureAsset->texture.data))
        textureData->assign(data, data + size);
      textureAsset->texture.range = ColorRange::LDR;
      stbi_image_free(data);
      spdlog::info("[AssetManager] loaded texture: {}", pathNormStr);
    } else
      spdlog::error("[AssetManager] failed to load texture: {}", pathNormStr);
  }
  return textureAsset;
}
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
