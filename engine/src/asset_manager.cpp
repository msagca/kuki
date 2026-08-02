#define GLM_ENABLE_EXPERIMENTAL
#include <animator.hpp>
#include <application.hpp>
#include <asset.hpp>
#include <asset_manager.hpp>
#include <asset_type.hpp>
#include <assimp/color4.h>
#include <assimp/material.h>
#include <assimp/matrix4x4.h>
#include <assimp/mesh.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <bounding_box.hpp>
#include <chrono>
#include <cmath>
#include <color.hpp>
#include <filesystem>
#include <future>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/ext/vector_int4.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <id.hpp>
#include <manager.hpp>
#include <material_asset.hpp>
#include <material_handle.hpp>
#include <material_type.hpp>
#include <memory>
#include <mesh_asset.hpp>
#include <mesh_handle.hpp>
#include <model_asset.hpp>
#include <scene.hpp>
#include <shader_asset.hpp>
#include <shader_type.hpp>
#include <skeleton.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <string>
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
  Load(Load<ShaderAsset>(vertId, vertPath));
  auto fragId = Register<ShaderAsset>(fragPath, std::move(name));
  auto asset = Load<ShaderAsset>(fragId, fragPath);
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
auto AssetManager::AssimpToGlmMat4(const aiMatrix4x4 &m) -> glm::mat4 {
  return {m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3, m.c3, m.d3, m.a4, m.b4, m.c4, m.d4};
}
auto AssetManager::AssimpTexToContent(const aiTextureType t) -> TextureContent {
  switch (t) {
  case aiTextureType_NORMALS:
    return TextureContent::Normal;
  case aiTextureType_METALNESS:
    return TextureContent::Metalness;
  case aiTextureType_AMBIENT_OCCLUSION:
    return TextureContent::Occlusion;
  case aiTextureType_DIFFUSE_ROUGHNESS:
    return TextureContent::Roughness;
  case aiTextureType_SPECULAR:
    return TextureContent::Specular;
  case aiTextureType_EMISSIVE:
    return TextureContent::Emissive;
  default:
    return TextureContent::Albedo;
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
auto AssetManager::LoadMaterial(std::unordered_map<std::string, unsigned int> &visited, const aiMaterial &aiMaterial, ModelAsset &model, const aiScene &aiScene, const std::filesystem::path &path, const std::string &fallbackName) -> unsigned int {
  const auto aiName = aiMaterial.GetName();
  const auto name = aiName.Empty() ? path.lexically_normal().string() : aiName.C_Str();
  if (auto it = visited.find(name); it != visited.end())
    return it->second;
  const unsigned int index = model.materials.size();
  visited.insert({name, index});
  model.materials.emplace_back();
  auto &material = model.materials.back();
  material.name = name;
  int shadingModel;
  if (aiMaterial.Get(AI_MATKEY_SHADING_MODEL, shadingModel) == AI_SUCCESS) {
    if (shadingModel == aiShadingMode_NoShading)
      material.type = MaterialType::Unlit;
    else
      material.type = MaterialType::Lit;
  }
  aiColor4D color;
  float value;
  if (aiMaterial.Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
    material.fallback.albedo = {color.r, color.g, color.b, color.a};
  if (aiMaterial.Get(AI_MATKEY_OPACITY, value) == AI_SUCCESS)
    material.fallback.albedo.w = value;
  if (aiMaterial.Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
    material.fallback.specular = {color.r, color.g, color.b, color.a};
  if (aiMaterial.Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
    material.fallback.emissive = {color.r, color.g, color.b, color.a};
  if (aiMaterial.Get(AI_MATKEY_METALLIC_FACTOR, value) == AI_SUCCESS || aiMaterial.Get(AI_MATKEY_REFLECTIVITY, value) == AI_SUCCESS)
    material.fallback.metalness = value;
  if (aiMaterial.Get(AI_MATKEY_ROUGHNESS_FACTOR, value) == AI_SUCCESS)
    material.fallback.roughness = value;
  else if (aiMaterial.Get(AI_MATKEY_SHININESS, value) == AI_SUCCESS)
    material.fallback.roughness = std::sqrt(2.f / (value + 2.f));
  if (!path.empty()) {
    LoadTexture(visited, aiMaterial, aiTextureType_AMBIENT_OCCLUSION, material, model, aiScene, path, fallbackName);
    LoadTexture(visited, aiMaterial, aiTextureType_DIFFUSE, material, model, aiScene, path, fallbackName);
    LoadTexture(visited, aiMaterial, aiTextureType_DIFFUSE_ROUGHNESS, material, model, aiScene, path, fallbackName);
    LoadTexture(visited, aiMaterial, aiTextureType_EMISSIVE, material, model, aiScene, path, fallbackName);
    LoadTexture(visited, aiMaterial, aiTextureType_METALNESS, material, model, aiScene, path, fallbackName);
    LoadTexture(visited, aiMaterial, aiTextureType_NORMALS, material, model, aiScene, path, fallbackName);
    LoadTexture(visited, aiMaterial, aiTextureType_SPECULAR, material, model, aiScene, path, fallbackName);
  }
  return index;
}
auto AssetManager::LoadMesh(const aiMesh &aiMesh, ModelAsset &model, BoundingBox &bounds, unsigned int parent, const std::string &fallbackName) -> unsigned int {
  const unsigned int index = model.meshes.size();
  model.meshes.emplace_back();
  auto &modelMesh = model.meshes.back();
  modelMesh.name = aiMesh.mName.Empty() ? fallbackName : aiMesh.mName.C_Str();
  modelMesh.parent = parent;
  for (auto i = 0; i < aiMesh.mNumVertices; ++i) {
    modelMesh.mesh.vertices.emplace_back();
    auto &vertex = modelMesh.mesh.vertices.back();
    vertex.position = glm::vec3(aiMesh.mVertices[i].x, aiMesh.mVertices[i].y, aiMesh.mVertices[i].z);
    bounds.min = glm::min(bounds.min, vertex.position);
    bounds.max = glm::max(bounds.max, vertex.position);
    if (aiMesh.mNormals)
      vertex.normal = glm::vec3(aiMesh.mNormals[i].x, aiMesh.mNormals[i].y, aiMesh.mNormals[i].z);
    if (aiMesh.mTangents)
      vertex.tangent = glm::vec3(aiMesh.mTangents[i].x, aiMesh.mTangents[i].y, aiMesh.mTangents[i].z);
    vertex.boneIds = glm::ivec4(-1); // NOTE: -1 indicates that this hasn't been assigned yet, will be used later
    if (aiMesh.mTextureCoords[0]) {
      glm::vec2 texCoord{};
      texCoord.x = aiMesh.mTextureCoords[0][i].x;
      texCoord.y = 1.f - aiMesh.mTextureCoords[0][i].y;
      vertex.texture = texCoord;
    } else
      vertex.texture = glm::vec2(0.f);
  }
  for (auto i = 0; i < aiMesh.mNumFaces; ++i) {
    const auto &face = aiMesh.mFaces[i];
    for (auto j = 0; j < face.mNumIndices; ++j)
      modelMesh.mesh.indices.push_back(face.mIndices[j]);
  }
  const auto vertexCount = modelMesh.mesh.vertices.size();
  std::unordered_map<std::string, int> boneNameToId;
  for (auto i = 0; i < aiMesh.mNumBones; ++i) {
    auto bone = aiMesh.mBones[i];
    const auto boneName(bone->mName.C_Str());
    auto boneId = 0u;
    if (auto it = boneNameToId.find(boneName); it != boneNameToId.end())
      boneId = it->second;
    else {
      boneId = static_cast<int>(boneNameToId.size());
      boneNameToId[boneName] = boneId;
      modelMesh.bones.push_back({.name = boneName, .offsetMatrix = AssimpToGlmMat4(bone->mOffsetMatrix)});
    }
    for (auto j = 0; j < bone->mNumWeights; ++j) {
      const auto vertexId = bone->mWeights[j].mVertexId;
      if (vertexId >= vertexCount)
        continue;
      const auto weight = bone->mWeights[j].mWeight;
      for (auto k = 0; k < 4; ++k)
        if (modelMesh.mesh.vertices[vertexId].boneIds[k] < 0) {
          modelMesh.mesh.vertices[vertexId].boneIds[k] = boneId;
          modelMesh.mesh.vertices[vertexId].boneWeights[k] = weight;
          break;
        }
    }
  }
  if (aiMesh.mNumBones > 0)
    for (auto &vertex : modelMesh.mesh.vertices)
      for (auto k = 0; k < 4; ++k)
        if (vertex.boneIds[k] < 0)
          vertex.boneIds[k] = 0;
  return index;
}
auto AssetManager::LoadNode(std::unordered_map<std::string, unsigned int> &visited, const aiNode &aiNode, const aiScene &aiScene, ModelAsset &model, const std::filesystem::path &path, const std::string &fallbackName, int parent) -> unsigned int {
  const unsigned int index = model.nodes.size();
  model.nodes.emplace_back();
  model.nodes[index].name = aiNode.mName.Empty() ? fallbackName : aiNode.mName.C_Str();
  model.nodes[index].transform = AssimpToGlmMat4(aiNode.mTransformation);
  model.nodes[index].parent = parent;
  for (auto i = 0; i < aiNode.mNumMeshes; ++i) {
    const auto meshId = aiNode.mMeshes[i];
    const auto aiMesh = aiScene.mMeshes[meshId];
    // TODO: keep track of loaded meshes and reuse the indices
    const auto meshIndex = LoadMesh(*aiMesh, model, model.nodes[index].bounds, index, fallbackName);
    model.nodes[index].meshes.push_back(meshIndex);
    auto &mesh = model.meshes.back();
    const auto matId = aiMesh->mMaterialIndex;
    if (matId >= 0 && matId < aiScene.mNumMaterials) {
      const auto aiMaterial = aiScene.mMaterials[matId];
      const auto materialIndex = LoadMaterial(visited, *aiMaterial, model, aiScene, path, fallbackName);
      mesh.material = materialIndex;
    }
  }
  for (auto i = 0; i < aiNode.mNumChildren; ++i) {
    const auto childIndex = LoadNode(visited, *aiNode.mChildren[i], aiScene, model, path, fallbackName, index);
    model.nodes[index].children.push_back(childIndex);
    const auto &childNode = model.nodes[childIndex];
    auto &node = model.nodes[index];
    node.bounds.min = glm::min(node.bounds.min, childNode.bounds.min);
    node.bounds.max = glm::max(node.bounds.max, childNode.bounds.max);
  }
  if (aiNode.mNumMeshes == 0 && aiNode.mNumChildren == 0)
    model.nodes[index].bounds = {.min = glm::vec3(.0f), .max = glm::vec3(.0f)};
  return index;
}
auto AssetManager::ParseAnimations(const aiScene &aiScene, ModelAsset &model) -> void {
  for (auto i = 0; i < aiScene.mNumAnimations; ++i) {
    const auto aiAnim = aiScene.mAnimations[i];
    model.animations.emplace_back();
    auto &clip = model.animations.back();
    clip.name = aiAnim->mName.Empty() ? ("Animation" + std::to_string(i)) : aiAnim->mName.C_Str();
    clip.duration = static_cast<float>(aiAnim->mDuration);
    clip.ticksPerSecond = aiAnim->mTicksPerSecond > 0.0 ? static_cast<float>(aiAnim->mTicksPerSecond) : 25.f;
    for (auto j = 0; j < aiAnim->mNumChannels; ++j) {
      const auto aiChannel = aiAnim->mChannels[j];
      clip.channels.emplace_back();
      auto &channel = clip.channels.back();
      channel.nodeName = aiChannel->mNodeName.C_Str();
      for (auto k = 0; k < aiChannel->mNumPositionKeys; ++k) {
        const auto &key = aiChannel->mPositionKeys[k];
        channel.positions.push_back({.time = static_cast<float>(key.mTime), .value = glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z)});
      }
      for (auto k = 0; k < aiChannel->mNumRotationKeys; ++k) {
        const auto &key = aiChannel->mRotationKeys[k];
        channel.rotations.push_back({.time = static_cast<float>(key.mTime), .value = glm::quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z)});
      }
      for (auto k = 0; k < aiChannel->mNumScalingKeys; ++k) {
        const auto &key = aiChannel->mScalingKeys[k];
        channel.scales.push_back({.time = static_cast<float>(key.mTime), .value = glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z)});
      }
    }
  }
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
auto AssetManager::LoadTexture(std::unordered_map<std::string, unsigned int> &visited, const aiMaterial &aiMaterial, const aiTextureType aiTextureType, ModelMaterial &material, ModelAsset &model, const aiScene &aiScene, const std::filesystem::path &path, const std::string &fallbackName) -> void {
  const auto count = aiMaterial.GetTextureCount(aiTextureType);
  const auto content = AssimpTexToContent(aiTextureType);
  for (auto i = 0; i < count; ++i) {
    const unsigned int index = model.textures.size();
    aiString texPath;
    if (aiMaterial.GetTexture(aiTextureType, i, &texPath) != AI_SUCCESS || texPath.Empty())
      continue;
    const auto embedded = aiScene.GetEmbeddedTexture(texPath.C_Str());
    const auto cacheKey = embedded ? path.lexically_normal().string() + "#" + texPath.C_Str() : (path / texPath.C_Str()).lexically_normal().string();
    if (auto it = visited.find(cacheKey); it != visited.end()) {
      material.textures.push_back(it->second);
      material.fallback.textureMask.set(static_cast<int>(content));
      continue;
    }
    visited.insert({cacheKey, index});
    ModelTexture modelTexture{};
    modelTexture.name = embedded ? fallbackName : cacheKey;
    modelTexture.texture.content = content;
    if (content == TextureContent::Albedo)
      // NOTE: we assume that albedo textures are, in general, in sRGB space
      modelTexture.texture.color = ColorSpace::sRGB;
    unsigned char *data = nullptr;
    if (embedded) {
      if (embedded->mHeight == 0)
        data = stbi_load_from_memory(reinterpret_cast<const unsigned char *>(embedded->pcData), static_cast<int>(embedded->mWidth), &modelTexture.texture.width, &modelTexture.texture.height, &modelTexture.texture.channels, 0);
      else
        spdlog::warn("[AssetManager] embedded texture '{}' uses uncompressed raw texel data, which is not currently supported", texPath.C_Str());
    } else
      data = stbi_load(cacheKey.c_str(), &modelTexture.texture.width, &modelTexture.texture.height, &modelTexture.texture.channels, 0);
    if (data) {
      const auto size = modelTexture.texture.width * modelTexture.texture.height * modelTexture.texture.channels;
      modelTexture.texture.data = std::vector<unsigned char>();
      if (auto textureData = std::get_if<std::vector<unsigned char>>(&modelTexture.texture.data))
        textureData->assign(data, data + size);
      stbi_image_free(data);
      material.textures.push_back(index);
      material.fallback.textureMask.set(static_cast<int>(modelTexture.texture.content));
      model.textures.push_back(modelTexture);
    }
  }
}
} // namespace kuki
