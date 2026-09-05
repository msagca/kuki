#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>
#include <animator.hpp>
#include <application.hpp>
#include <asset.hpp>
#include <asset_manager.hpp>
#include <asset_type.hpp>
#include <assimp/Importer.hpp>
#include <assimp/color4.h>
#include <assimp/material.h>
#include <assimp/matrix4x4.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <bounding_box.hpp>
#include <chrono>
#include <cmath>
#include <color.hpp>
#include <filesystem>
#include <fstream>
#include <future>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/ext/vector_int4.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <id.hpp>
#include <limits>
#include <manager.hpp>
#include <material_asset.hpp>
#include <material_handle.hpp>
#include <material_type.hpp>
#include <memory>
#include <mesh_asset.hpp>
#include <mesh_handle.hpp>
#include <model_asset.hpp>
#include <model_import.hpp>
#include <nlohmann/json.hpp>
#include <scene.hpp>
#include <shader_asset.hpp>
#include <shader_type.hpp>
#include <skeleton.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <string>
#include <texture.hpp>
#include <texture_asset.hpp>
#include <texture_content.hpp>
#include <transform.hpp>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
namespace kuki {
const std::unordered_map<std::type_index, AssetType> AssetManager::typeIndexToAssetType = {
  {typeid(MaterialAsset), AssetType::Material},
  {typeid(MeshAsset), AssetType::Mesh},
  {typeid(ModelAsset), AssetType::Model},
  {typeid(ShaderAsset), AssetType::Shader},
  {typeid(TextureAsset), AssetType::Texture}};
AssetManager::AssetManager(Application &app)
  : Manager(app) {}
auto AssetManager::Add(std::unique_ptr<Asset> asset, std::string name) -> bool {
  if (!asset || !asset->id)
    return false;
  const auto id = asset->id;
  if (auto it = idToAsset.find(id); it != idToAsset.end())
    return false;
  typeIndexToAssetSet[asset->GetTypeIndex()].insert(id);
  const auto shaderAsset = asset->As<ShaderAsset>();
  const auto isVertexShader = shaderAsset && shaderAsset->shaderType == ShaderType::Vertex;
  idToAsset.emplace(id, std::move(asset));
  if (!isVertexShader)
    spdlog::info("[AssetManager] created: {}", name);
  SetName(id, std::move(name));
  return true;
}
auto AssetManager::GetName(const AssetID id) const -> std::string {
  if (auto it = idToName.find(id); it != idToName.end())
    return it->second;
  return "";
}
auto AssetManager::GetType(const AssetID id) const -> AssetType {
  if (auto it = idToAsset.find(id); it != idToAsset.end())
    return it->second->GetType();
  for (const auto &[typeIndex, ids] : typeIndexToAssetSet)
    if (ids.contains(id))
      return Asset::GetType(typeIndex);
  return AssetType::Texture;
}
auto AssetManager::GetPath(const AssetID id) const -> std::filesystem::path {
  if (auto it = idToPath.find(id); it != idToPath.end())
    return it->second;
  return {};
}
auto AssetManager::IsLoaded(const AssetID id) const -> bool {
  return idToAsset.contains(id);
}
auto AssetManager::CreatePrefab(const AssetID assetId) -> EntityID {
  if (auto it = idToFuture.find(assetId); it != idToFuture.end()) {
    auto asset = it->second.get();
    idToFuture.erase(it);
    Load(std::move(asset));
  }
  if (auto it = idToAsset.find(assetId); it != idToAsset.end()) {
    if (auto it2 = idToPrefabId.find(assetId); it2 != idToPrefabId.end())
      return it2->second;
    const auto asset = it->second.get();
    const auto type = asset->GetType();
    auto prefabId = EntityID::Invalid;
    switch (type) {
    case AssetType::Material:
      prefabId = CreatePrefab<MaterialAsset>(assetId);
      break;
    case AssetType::Mesh:
      prefabId = CreatePrefab<MeshAsset>(assetId);
      break;
    case AssetType::Model:
      prefabId = CreatePrefab<ModelAsset>(assetId);
      break;
    case AssetType::Shader:
      prefabId = CreatePrefab<ShaderAsset>(assetId);
      break;
    case AssetType::Texture:
      prefabId = CreatePrefab<TextureAsset>(assetId);
      break;
    default:
      break;
    }
    if (prefabId) {
      idToPrefabId.emplace(assetId, prefabId);
      return prefabId;
    }
  }
  return EntityID::Invalid;
}
auto AssetManager::GetPrefabID(const AssetID id) const -> EntityID {
  if (auto it = idToPrefabId.find(id); it != idToPrefabId.end())
    return it->second;
  return EntityID::Invalid;
}
auto AssetManager::Rename(const AssetID id, std::string nameNew) -> bool {
  if (auto it = idToName.find(id); it != idToName.end()) {
    const auto &nameOld = it->second;
    auto ids = nameToId.equal_range(nameOld);
    for (auto &it2 = ids.first; it2 != ids.second;) {
      if (it2->second == id) {
        it2 = nameToId.erase(it2);
        break;
      } else
        ++it2;
    }
    idToName.erase(it);
    SetName(id, std::move(nameNew));
    return true;
  } else if (idToAsset.contains(id)) {
    SetName(id, std::move(nameNew));
    return true;
  }
  return false;
}
auto AssetManager::Instantiate(const AssetID assetId, Scene &scene) -> EntityID {
  const auto prefabId = CreatePrefab(assetId);
  if (!prefabId)
    return EntityID::Invalid;
  auto entityId = scene.CopyEntityFrom(prefabManager, prefabId);
  if (auto modelAsset = Get<ModelAsset>(assetId); modelAsset)
    ResolveModelInstance(scene, entityId, *modelAsset, assetId);
  return entityId;
}
auto AssetManager::ResolveModelInstance(Scene &scene, const EntityID root, const ModelAsset &modelAsset, const AssetID modelAssetId) -> void {
  if (modelAsset.animations.empty() || !root)
    return;
  std::unordered_map<std::string, EntityID> nameToEntityId;
  const auto Walk = [&](const EntityID id, const auto &self) -> void {
    nameToEntityId.emplace(scene.GetEntityName(id), id);
    scene.ForEachChildEntity(id, [&](const EntityID childId) {
      self(childId, self);
    });
  };
  Walk(root, Walk);
  std::vector<EntityID> nodeEntities(modelAsset.nodes.size(), EntityID::Invalid);
  for (auto i = 0u; i < modelAsset.nodes.size(); ++i)
    if (auto it = nameToEntityId.find(modelAsset.nodes[i].name); it != nameToEntityId.end())
      nodeEntities[i] = it->second;
  auto *skeleton = scene.AddEntityComponent<Skeleton>(root);
  if (skeleton)
    skeleton->nodeEntities = std::move(nodeEntities);
  auto *animator = scene.AddEntityComponent<Animator>(root);
  if (animator) {
    animator->modelAssetId = modelAssetId;
    animator->clipIndex = 0;
    animator->time = 0.f;
    animator->playing = true;
    animator->loop = true;
  }
}
auto AssetManager::Instantiate(const std::string &name, Scene &scene) -> EntityID {
  if (auto ids = nameToId.equal_range(name); ids.first != ids.second)
    return Instantiate(ids.first->second, scene);
  return EntityID::Invalid;
}
auto AssetManager::LoadComputeFromSource(const std::string_view source, std::string name) -> AssetID {
  const auto id = AssetID::Generate();
  auto asset = std::make_unique<ShaderAsset>(id);
  asset->shaderType = ShaderType::Compute;
  asset->text = std::string(source);
  Add(std::move(asset), std::move(name));
  return id;
}
auto AssetManager::LoadShader(const std::filesystem::path &vertPath, const std::filesystem::path &fragPath, std::string name, const MaterialType materialType) -> AssetID {
  auto vertId = Register<ShaderAsset>(vertPath, "");
  Load(Load<ShaderAsset>(vertId, ResolvePath(vertPath)));
  auto fragId = Register<ShaderAsset>(fragPath, std::move(name));
  auto asset = Load<ShaderAsset>(fragId, ResolvePath(fragPath));
  if (auto fragAsset = asset->As<ShaderAsset>(); fragAsset) {
    fragAsset->materialType = materialType;
    fragAsset->vertexShader = vertId;
    Load(std::move(asset));
  }
  return fragId;
}
auto AssetManager::LoadShaderFromSource(const std::string_view vertSource, const std::string_view fragSource, std::string name, const MaterialType materialType) -> AssetID {
  const auto vertId = AssetID::Generate();
  auto vertAsset = std::make_unique<ShaderAsset>(vertId);
  vertAsset->shaderType = ShaderType::Vertex;
  vertAsset->text = std::string(vertSource);
  Add(std::move(vertAsset));
  const auto fragId = AssetID::Generate();
  auto fragAsset = std::make_unique<ShaderAsset>(fragId);
  fragAsset->shaderType = ShaderType::Fragment;
  fragAsset->text = std::string(fragSource);
  fragAsset->materialType = materialType;
  fragAsset->vertexShader = vertId;
  Add(std::move(fragAsset), std::move(name));
  return fragId;
}
auto AssetManager::Update() -> void {
  for (auto it = idToFuture.begin(); it != idToFuture.end();)
    if (it->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
      loadedAssets.emplace(it->second.get());
      it = idToFuture.erase(it);
    } else
      ++it;
  while (!loadedAssets.empty()) {
    Load(std::move(loadedAssets.front()));
    loadedAssets.pop();
  }
}
auto AssetManager::CreateNodePrefab(const AssetID assetId, const ModelAsset &modelAsset, const int nodeIndex) -> EntityID {
  if (nodeIndex >= modelAsset.nodes.size())
    return EntityID::Invalid;
  const auto &node = modelAsset.nodes[nodeIndex];
  const auto prefabId = prefabManager.Create(node.name);
  auto transform = prefabManager.AddComponent<Transform>(prefabId);
  transform->local = node.transform;
  glm::vec3 skew;
  glm::vec4 perspective;
  glm::decompose(node.transform, transform->scale, transform->rotation, transform->position, skew, perspective);
  if (node.bounds) {
    auto bounds = prefabManager.AddComponent<BoundingBox>(prefabId);
    *bounds = node.bounds;
  }
  const auto AddComponentHandles = [&](const EntityID nodeId, const int meshIndex) {
    if (meshIndex < 0 || meshIndex >= modelAsset.meshes.size())
      return;
    auto meshHandle = prefabManager.AddComponent<ModelMeshHandle>(nodeId);
    meshHandle->modelAssetId = assetId;
    meshHandle->meshIndex = meshIndex;
    const auto materialIndex = modelAsset.meshes[meshIndex].material;
    if (materialIndex < 0 || materialIndex >= modelAsset.materials.size())
      return;
    auto materialHandle = prefabManager.AddComponent<ModelMaterialHandle>(nodeId);
    materialHandle->modelAssetId = assetId;
    materialHandle->materialIndex = materialIndex;
    const auto &material = modelAsset.materials[materialIndex];
    for (auto i = 0; i < material.textures.size(); ++i) {
      const auto textureIndex = material.textures[i];
      materialHandle->textureIndices.push_back(textureIndex);
    }
  };
  if (node.meshes.size() > 1)
    for (auto i = 0; i < node.meshes.size(); ++i) {
      const auto childId = prefabManager.Create();
      auto childTransform = prefabManager.AddComponent<Transform>(childId);
      childTransform->world = node.transform;
      prefabManager.AddChild(prefabId, childId);
      const auto meshIndex = node.meshes[i];
      AddComponentHandles(childId, meshIndex);
    }
  else if (node.meshes.size() == 1) {
    const auto meshIndex = node.meshes[0];
    AddComponentHandles(prefabId, meshIndex);
  }
  for (auto i = 0; i < node.children.size(); ++i) {
    const auto childIndex = node.children[i];
    const auto childId = CreateNodePrefab(assetId, modelAsset, childIndex);
    prefabManager.AddChild(prefabId, childId);
  }
  return prefabId;
}
auto AssetManager::Load(std::unique_ptr<Asset> asset) -> void {
  const auto typeIndex = asset->GetTypeIndex();
  const auto id = asset->id;
  typeIndexToAssetSet[typeIndex].insert(id);
  const auto modelAsset = asset->As<ModelAsset>();
  idToAsset.insert_or_assign(id, std::move(asset));
  if (modelAsset)
    RegisterModelTextures(*modelAsset);
}
auto AssetManager::RegisterModelTextures(const ModelAsset &modelAsset) -> void {
  const auto modelName = GetName(modelAsset.id);
  for (size_t i = 0; i < modelAsset.textures.size(); ++i) {
    const auto &modelTexture = modelAsset.textures[i];
    auto textureAsset = std::make_unique<TextureAsset>(AssetID::Generate());
    textureAsset->modelAssetId = modelAsset.id;
    textureAsset->modelTextureIndex = i;
    auto stem = std::filesystem::path(modelTexture.name).stem().string();
    if (stem.empty())
      stem = modelTexture.name;
    Add(std::move(textureAsset), modelName.empty() ? stem : modelName + "/" + stem);
  }
}
auto AssetManager::SetName(const AssetID id, std::string name) -> void {
  if (name.empty())
    return;
  idToName[id] = name;
  nameToId.emplace(std::move(name), id);
}
auto AssetManager::ResolveNodeReferences(ModelAsset &model) -> void {
  std::unordered_map<std::string, unsigned int> nameToNodeIndex;
  for (auto i = 0; i < model.nodes.size(); ++i)
    nameToNodeIndex.emplace(model.nodes[i].name, static_cast<unsigned int>(i));
  for (auto &mesh : model.meshes)
    for (auto &bone : mesh.bones)
      if (auto it = nameToNodeIndex.find(bone.name); it != nameToNodeIndex.end())
        bone.nodeIndex = it->second;
  for (auto &clip : model.animations)
    for (auto &channel : clip.channels)
      if (auto it = nameToNodeIndex.find(channel.nodeName); it != nameToNodeIndex.end())
        channel.nodeIndex = it->second;
}
/// @brief Marks a `visited` entry as a texture that could not be loaded, so it is not retried.
///
/// Distinct from an absent entry, which means "not seen yet", and from any real index.
template <>
auto AssetManager::Load<ModelAsset>(const AssetID id, const std::filesystem::path &path) -> std::unique_ptr<Asset> {
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
  // the scene knows both counts, and growing these while a mesh reference is live is worth avoiding
  model->meshes.reserve(aiScene->mNumMeshes);
  model->materials.reserve(aiScene->mNumMaterials);
  std::unordered_map<std::string, unsigned int> visited;
  LoadNode(visited, *aiScene->mRootNode, *aiScene, *model.get(), path.parent_path(), path.filename().string());
  ParseAnimations(*aiScene, *model.get());
  ResolveNodeReferences(*model.get());
  spdlog::info("[AssetManager] loaded model: {}", pathNormStr);
  return model;
}
template <>
auto AssetManager::Load<ShaderAsset>(const AssetID id, const std::filesystem::path &path) -> std::unique_ptr<Asset> {
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
auto AssetManager::Load<MaterialAsset>(const AssetID id, const std::filesystem::path &path) -> std::unique_ptr<Asset> {
  const auto pathNormStr = path.lexically_normal().string();
  auto material = std::make_unique<MaterialAsset>(id);
  material->type = MaterialType::Lit;
  std::ifstream fs(path);
  if (!fs) {
    spdlog::error("[AssetManager] failed to open material file: {}", pathNormStr);
    return material;
  }
  nlohmann::json description;
  try {
    fs >> description;
  } catch (const nlohmann::json::parse_error &e) {
    spdlog::error("[AssetManager] failed to parse material file: {} ({})", pathNormStr, e.what());
    return material;
  }
  const auto ReadColor = [&description](const char *name, glm::vec4 &target) {
    if (!description.contains(name))
      return;
    const auto &value = description.at(name);
    if (!value.is_array())
      return;
    for (size_t i = 0; i < value.size() && i < 4; ++i)
      target[static_cast<glm::length_t>(i)] = value[i].get<float>();
  };
  ReadColor("albedo", material->fallback.albedo);
  ReadColor("specular", material->fallback.specular);
  ReadColor("emissive", material->fallback.emissive);
  ReadColor("attenuationColor", material->fallback.attenuation);
  const auto alphaMode = description.value("alphaMode", std::string{});
  if (alphaMode == "mask")
    material->fallback.alphaMode = AlphaMode::Mask;
  else if (alphaMode == "blend")
    material->fallback.alphaMode = AlphaMode::Blend;
  else if (alphaMode.empty() && material->fallback.albedo.w < 1.f)
    material->fallback.alphaMode = AlphaMode::Blend;
  material->fallback.alphaCutoff = description.value("alphaCutoff", material->fallback.alphaCutoff);
  material->fallback.metalness = description.value("metalness", material->fallback.metalness);
  material->fallback.occlusion = description.value("occlusion", material->fallback.occlusion);
  material->fallback.roughness = description.value("roughness", material->fallback.roughness);
  material->fallback.transmission = description.value("transmission", material->fallback.transmission);
  material->fallback.thickness = description.value("thickness", material->fallback.thickness);
  material->fallback.ior = description.value("ior", material->fallback.ior);
  material->fallback.attenuation.w = description.value("attenuationDistance", material->fallback.attenuation.w);
  if (description.value("unlit", false))
    material->type = MaterialType::Unlit;
  spdlog::info("[AssetManager] loaded material: {}", pathNormStr);
  return material;
}
template <>
auto AssetManager::Load<TextureAsset>(const AssetID id, const std::filesystem::path &path) -> std::unique_ptr<Asset> {
  const auto pathNormStr = path.lexically_normal().string();
  auto textureAsset = std::make_unique<TextureAsset>(id);
  auto &texture = textureAsset->texture;
  texture.source = pathNormStr;
  if (!ReadTexturePixels(texture))
    return textureAsset;
  if (texture.range == ColorRange::HDR) {
    texture.content = TextureContent::Skybox;
    if (path.extension() == ".exr")
      texture.flipY = true;
  }
  spdlog::info("[AssetManager] loaded texture: {}", pathNormStr);
  return textureAsset;
}
} // namespace kuki
