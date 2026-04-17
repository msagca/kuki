#include <application.hpp>
#include <asset_manager.hpp>
#include <asset_metadata.hpp>
#include <bounding_box.hpp>
#include <chrono>
#include <filesystem>
#include <future>
#include <id.hpp>
#include <material_asset.hpp>
#include <material_handle.hpp>
#include <memory>
#include <mesh_asset.hpp>
#include <mesh_handle.hpp>
#include <scene_asset.hpp>
#include <skybox_asset.hpp>
#include <stb_image.h>
#include <texture_asset.hpp>
namespace kuki {
const std::unordered_map<std::type_index, AssetType> AssetManager::typeIndexToAssetType = {
  {typeid(MaterialAsset), AssetType::Material},
  {typeid(MeshAsset), AssetType::Mesh},
  {typeid(SceneAsset), AssetType::Scene},
  {typeid(ShaderAsset), AssetType::Shader},
  {typeid(SkyboxAsset), AssetType::Skybox},
  {typeid(TextureAsset), AssetType::Texture}};
auto AssetManager::GetID(const std::string &name) const -> AssetID {
  auto ids = nameToId.equal_range(name);
  if (auto it = ids.first; it != ids.second)
    return it->second;
  return AssetID::Invalid;
}
auto AssetManager::GetName(const AssetID id) const -> std::string {
  if (auto it = idToMetadata.find(id); it != idToMetadata.end())
    return it->second.name;
  return "";
}
auto AssetManager::GetPath(const AssetID id) const -> std::filesystem::path {
  if (auto it = idToMetadata.find(id); it != idToMetadata.end())
    return it->second.path;
  return std::filesystem::path{};
}
auto AssetManager::GetStatus(const AssetID id) const -> AssetStatus {
  if (auto it = idToMetadata.find(id); it != idToMetadata.end())
    return it->second.status;
  return AssetStatus::Unregistered;
}
auto AssetManager::GetType(const AssetID id) const -> AssetType {
  if (auto it = idToMetadata.find(id); it != idToMetadata.end())
    return it->second.type;
  return AssetType::Texture;
}
auto AssetManager::IsRegistered(const AssetID id) const -> bool {
  return idToMetadata.contains(id);
}
auto AssetManager::SetStatus(const AssetID id, const AssetStatus status) -> bool {
  if (status == AssetStatus::Unregistered)
    return false;
  if (auto it = idToMetadata.find(id); it != idToMetadata.end()) {
    it->second.status = status;
    return true;
  }
  return false;
}
auto AssetManager::CreatePrefab(const AssetID assetId) -> EntityID {
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
    case AssetType::Scene:
      prefabId = CreatePrefab<SceneAsset>(assetId);
      break;
    case AssetType::Shader:
      prefabId = CreatePrefab<ShaderAsset>(assetId);
      break;
    case AssetType::Skybox:
      prefabId = CreatePrefab<SkyboxAsset>(assetId);
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
auto AssetManager::Instantiate(const AssetID id, Scene &scene) -> EntityID {
  const auto prefabId = CreatePrefab(id);
  if (!prefabId)
    return EntityID::Invalid;
  return scene.CopyEntityFrom(prefabManager, prefabId);
}
auto AssetManager::Instantiate(const std::string &name, Scene &scene) -> EntityID {
  const auto assetId = GetID(name);
  return Instantiate(assetId, scene);
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
auto AssetManager::CreateNodePrefab(const AssetID assetId, const SceneAsset &sceneAsset, const int nodeIndex) -> EntityID {
  if (nodeIndex >= sceneAsset.nodes.size())
    return EntityID::Invalid;
  const auto &node = sceneAsset.nodes[nodeIndex];
  const auto name = node.name;
  const auto prefabId = prefabManager.Create(name);
  auto transform = prefabManager.AddComponent<Transform>(prefabId);
  transform->local = node.transform;
  if (node.bounds) {
    auto bounds = prefabManager.AddComponent<BoundingBox>(prefabId);
    *bounds = node.bounds;
  }
  const auto AddComponentHandles = [&](const EntityID nodeId, const int meshIndex) {
    if (meshIndex < 0 || meshIndex >= sceneAsset.meshes.size())
      return;
    auto meshHandle = prefabManager.AddComponent<SceneMeshHandle>(nodeId);
    meshHandle->sceneAssetId = assetId;
    meshHandle->meshIndex = meshIndex;
    const auto materialIndex = sceneAsset.meshes[meshIndex].material;
    if (materialIndex < 0 || materialIndex >= sceneAsset.materials.size())
      return;
    auto materialHandle = prefabManager.AddComponent<SceneMaterialHandle>(nodeId);
    materialHandle->sceneAssetId = assetId;
    materialHandle->materialIndex = materialIndex;
    const auto &material = sceneAsset.materials[materialIndex];
    for (auto i = 0; i < material.textures.size(); ++i) {
      const auto textureIndex = material.textures[i];
      materialHandle->textureIndices.push_back(textureIndex);
    }
  };
  if (node.meshes.size() > 1) // create a child entity for each mesh
    for (auto i = 0; i < node.meshes.size(); ++i) {
      const auto childId = prefabManager.Create();
      auto childTransform = prefabManager.AddComponent<Transform>(childId);
      childTransform->world = node.transform;
      prefabManager.AddChild(prefabId, childId);
      const auto meshIndex = node.meshes[i];
      AddComponentHandles(childId, meshIndex);
    }
  else if (node.meshes.size() == 1) { // attach mesh and material components to this node
    const auto meshIndex = node.meshes[0];
    AddComponentHandles(prefabId, meshIndex);
  }
  for (auto i = 0; i < node.children.size(); ++i) {
    const auto childIndex = node.children[i];
    const auto childId = CreateNodePrefab(assetId, sceneAsset, childIndex);
    prefabManager.AddChild(prefabId, childId);
  }
  return prefabId;
}
auto AssetManager::Load(std::unique_ptr<Asset> asset) -> void {
  const auto typeIndex = asset->GetTypeIndex();
  const auto id = asset->id;
  typeIndexToAssetSet[typeIndex].insert(id);
  idToAsset.insert_or_assign(id, std::move(asset));
  SetStatus(id, AssetStatus::Loaded);
}
auto AssetManager::LoadMaterial(std::unordered_map<std::string, unsigned int> &visited, const aiMaterial &aiMaterial, SceneAsset &scene, const std::filesystem::path &path) -> unsigned int {
  const auto aiName = aiMaterial.GetName();
  const auto name = aiName.Empty() ? path.lexically_normal().string() : aiName.C_Str();
  if (auto it = visited.find(name); it != visited.end())
    return it->second;
  const unsigned int index = scene.materials.size();
  visited.insert({name, index});
  scene.materials.emplace_back();
  auto &material = scene.materials.back();
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
  if (aiMaterial.Get(AI_MATKEY_COLOR_AMBIENT, value) == AI_SUCCESS)
    material.fallback.occlusion = value;
  if (aiMaterial.Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
    material.fallback.emissive = {color.r, color.g, color.b, color.a};
  if (aiMaterial.Get(AI_MATKEY_METALLIC_FACTOR, value) == AI_SUCCESS || aiMaterial.Get(AI_MATKEY_REFLECTIVITY, value) == AI_SUCCESS)
    material.fallback.metalness = value;
  if (aiMaterial.Get(AI_MATKEY_ROUGHNESS_FACTOR, value) == AI_SUCCESS)
    material.fallback.roughness = value;
  else if (aiMaterial.Get(AI_MATKEY_SHININESS, value) == AI_SUCCESS)
    material.fallback.roughness = std::sqrt(2.f / (value + 2.f));
  if (!path.empty()) {
    LoadTexture(visited, aiMaterial, aiTextureType_AMBIENT_OCCLUSION, material, scene, path);
    LoadTexture(visited, aiMaterial, aiTextureType_DIFFUSE, material, scene, path);
    LoadTexture(visited, aiMaterial, aiTextureType_DIFFUSE_ROUGHNESS, material, scene, path);
    LoadTexture(visited, aiMaterial, aiTextureType_EMISSIVE, material, scene, path);
    LoadTexture(visited, aiMaterial, aiTextureType_METALNESS, material, scene, path);
    LoadTexture(visited, aiMaterial, aiTextureType_NORMALS, material, scene, path);
    LoadTexture(visited, aiMaterial, aiTextureType_SPECULAR, material, scene, path);
  }
  return index;
}
auto AssetManager::LoadMesh(const aiMesh &aiMesh, SceneAsset &scene, BoundingBox &bounds, unsigned int parent) -> unsigned int {
  const unsigned int index = scene.meshes.size();
  scene.meshes.emplace_back();
  auto &sceneMesh = scene.meshes.back();
  sceneMesh.name = aiMesh.mName.C_Str();
  sceneMesh.parent = parent;
  for (auto i = 0; i < aiMesh.mNumVertices; ++i) {
    sceneMesh.mesh.vertices.emplace_back();
    auto &vertex = sceneMesh.mesh.vertices.back();
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
      sceneMesh.mesh.indices.push_back(face.mIndices[j]);
  }
  const auto vertexCount = sceneMesh.mesh.vertices.size();
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
    }
    for (auto j = 0; j < bone->mNumWeights; ++j) {
      const auto vertexId = bone->mWeights[j].mVertexId;
      if (vertexId >= vertexCount)
        continue;
      const auto weight = bone->mWeights[j].mWeight;
      for (auto k = 0; k < 4; ++k)
        // assign the weight to the next unassigned id
        if (sceneMesh.mesh.vertices[vertexId].boneIds[k] < 0) {
          sceneMesh.mesh.vertices[vertexId].boneIds[k] = boneId;
          sceneMesh.mesh.vertices[vertexId].boneWeights[k] = weight;
          break;
        }
    }
  }
  return index;
}
auto AssetManager::LoadNode(std::unordered_map<std::string, unsigned int> &visited, const aiNode &aiNode, const aiScene &aiScene, SceneAsset &scene, const std::filesystem::path &path, int parent) -> unsigned int {
  const unsigned int index = scene.nodes.size();
  scene.nodes.emplace_back();
  scene.nodes[index].name = aiNode.mName.C_Str();
  scene.nodes[index].transform = AssimpToGlmMat4(aiNode.mTransformation);
  scene.nodes[index].parent = parent;
  for (auto i = 0; i < aiNode.mNumMeshes; ++i) {
    const auto meshId = aiNode.mMeshes[i];
    const auto aiMesh = aiScene.mMeshes[meshId];
    const auto meshIndex = LoadMesh(*aiMesh, scene, scene.nodes[index].bounds, index);
    scene.nodes[index].meshes.push_back(meshIndex);
    auto &mesh = scene.meshes.back();
    const auto matId = aiMesh->mMaterialIndex;
    if (matId >= 0 && matId < aiScene.mNumMaterials) {
      const auto aiMaterial = aiScene.mMaterials[matId];
      const auto materialIndex = LoadMaterial(visited, *aiMaterial, scene, path);
      mesh.material = materialIndex;
    }
  }
  for (auto i = 0; i < aiNode.mNumChildren; ++i) {
    const auto childIndex = LoadNode(visited, *aiNode.mChildren[i], aiScene, scene, path, index);
    scene.nodes[index].children.push_back(childIndex);
    const auto &childNode = scene.nodes[childIndex];
    auto &curNode = scene.nodes[index];
    curNode.bounds.min = glm::min(curNode.bounds.min, childNode.bounds.min);
    curNode.bounds.max = glm::max(curNode.bounds.max, childNode.bounds.max);
  }
  if (aiNode.mNumMeshes == 0 && aiNode.mNumChildren == 0)
    scene.nodes[index].bounds = {.min = glm::vec3(.0f), .max = glm::vec3(.0f)};
  else if (parent >= 0)
    scene.nodes[index].bounds = scene.nodes[index].bounds.GetWorldBounds(scene.nodes[parent].transform * scene.nodes[index].transform);
  return index;
}
auto AssetManager::LoadTexture(std::unordered_map<std::string, unsigned int> &visited, const aiMaterial &aiMaterial, const aiTextureType aiTextureType, SceneMaterial &material, SceneAsset &scene, const std::filesystem::path &path) -> void {
  const auto count = aiMaterial.GetTextureCount(aiTextureType);
  const auto content = AssimpTexToContent(aiTextureType);
  for (auto i = 0; i < count; ++i) {
    const unsigned int index = scene.textures.size();
    aiString texPath;
    aiMaterial.GetTexture(aiTextureType, i, &texPath);
    const auto fullPath = path / texPath.C_Str();
    const auto pathNormStr = fullPath.lexically_normal().string();
    if (auto it = visited.find(pathNormStr); it != visited.end()) {
      material.textures.push_back(it->second);
      material.fallback.textureMask.set(static_cast<int>(content));
      continue;
    }
    visited.insert({pathNormStr, index});
    SceneTexture sceneTexture{};
    sceneTexture.name = pathNormStr;
    sceneTexture.texture.content = content;
    if (auto data = stbi_load(pathNormStr.c_str(), &sceneTexture.texture.width, &sceneTexture.texture.height, &sceneTexture.texture.channels, 0); data) {
      const auto size = sceneTexture.texture.width * sceneTexture.texture.height * sceneTexture.texture.channels;
      sceneTexture.texture.data.assign(data, data + size);
      stbi_image_free(data);
      material.textures.push_back(index);
      material.fallback.textureMask.set(static_cast<int>(sceneTexture.texture.content));
      scene.textures.push_back(sceneTexture);
    }
  }
}
} // namespace kuki
