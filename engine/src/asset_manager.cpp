#include <asset_manager.hpp>
#include <asset_metadata.hpp>
#include <chrono>
#include <filesystem>
#include <future>
#include <material_handle.hpp>
#include <memory>
#include <mesh_handle.hpp>
#include <scene_asset.hpp>
#include <skybox_asset.hpp>
#include <stb_image.h>
namespace kuki {
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
auto AssetManager::GetName(const AssetID id) const -> std::string {
  return assetDb.GetName(id);
}
auto AssetManager::GetPath(const AssetID id) const -> std::filesystem::path {
  return assetDb.GetPath(id);
}
auto AssetManager::Instantiate(const AssetID id, Scene &scene) -> EntityID {
  const auto prefabId = CreatePrefab(id);
  if (!prefabId)
    return EntityID::Invalid;
  return scene.CopyEntityFrom(prefabManager, prefabId);
}
auto AssetManager::Unload(const AssetID id) -> bool {
  // TODO: release GPU resources if applicable
  assetDb.SetStatus(id, AssetStatus::Registered);
  if (auto it = idToAsset.find(id); it != idToAsset.end()) {
    const auto type = it->second.get()->GetTypeIndex();
    if (auto it2 = typeIndexToAssetSet.find(type); it2 != typeIndexToAssetSet.end())
      it2->second.erase(id);
  }
  if (auto it = idToPrefabId.find(id); it != idToPrefabId.end()) {
    prefabManager.Delete(it->second);
    idToPrefabId.erase(it);
  }
  return idToAsset.erase(id);
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
  case aiTextureType_DIFFUSE:
    return TextureContent::Albedo;
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
    return TextureContent::Unknown;
  }
}
auto AssetManager::CreateNodePrefab(const AssetID assetId, const SceneAsset &sceneAsset, const int nodeIndex, const EntityID parentPrefab) -> EntityID {
  if (nodeIndex >= sceneAsset.nodes.size())
    return EntityID::Invalid;
  const auto &node = sceneAsset.nodes[nodeIndex];
  if (prefabManager.IsEntity(node.name))
    return prefabManager.GetID(node.name);
  const auto prefabId = prefabManager.Create(node.name);
  auto transform = prefabManager.AddComponent<Transform>(prefabId);
  transform->world = node.transform;
  const auto AddComponentHandles = [&](const EntityID nodeId, const int meshIndex) {
    auto meshHandle = prefabManager.AddComponent<MeshHandle>(nodeId);
    meshHandle->sceneAssetId = assetId;
    meshHandle->meshIndex = meshIndex;
    const auto &materialIndex = sceneAsset.meshes[meshIndex].material;
    if (materialIndex < 0 || materialIndex >= sceneAsset.materials.size())
      return;
    auto materialHandle = prefabManager.AddComponent<MaterialHandle>(nodeId);
    materialHandle->sceneAssetId = assetId;
    materialHandle->materialIndex = materialIndex;
    const auto &material = sceneAsset.materials[materialIndex];
    for (auto i = 0; i < material.textures.size(); ++i) {
      const auto &textureIndex = material.textures[i];
      materialHandle->textureIndices.push_back(textureIndex);
    }
  };
  if (node.meshes.size() > 1) // create a child entity for each mesh
    for (auto i = 0; i < node.meshes.size(); ++i) {
      const auto childId = prefabManager.Create();
      auto childTransform = prefabManager.AddComponent<Transform>(childId);
      childTransform->world = node.transform;
      prefabManager.AddChild(prefabId, childId);
      const auto &meshIndex = node.meshes[i];
      AddComponentHandles(childId, meshIndex);
    }
  else if (node.meshes.size() == 1) { // attach mesh and material components to this node
    const auto &meshIndex = node.meshes[0];
    AddComponentHandles(prefabId, meshIndex);
  }
  for (auto i = 0; i < node.children.size(); ++i) {
    const auto &childIndex = node.children[i];
    const auto childId = CreateNodePrefab(assetId, sceneAsset, childIndex, prefabId);
    prefabManager.AddChild(prefabId, childId);
  }
  return prefabId;
}
auto AssetManager::CreatePrefab(const AssetID assetId) -> EntityID {
  if (auto it = idToAsset.find(assetId); it != idToAsset.end()) {
    if (auto it2 = idToPrefabId.find(assetId); it2 != idToPrefabId.end())
      return it2->second;
    const auto asset = it->second.get();
    const auto type = asset->GetType();
    auto prefabId = EntityID::Invalid;
    switch (type) {
    case AssetType::Scene:
      prefabId = CreatePrefab<SceneAsset>(assetId);
      break;
    case AssetType::Skybox:
      prefabId = CreatePrefab<SkyboxAsset>(assetId);
      break;
    case AssetType::Shader:
      prefabId = CreatePrefab<ShaderAsset>(assetId);
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
auto AssetManager::Load(std::unique_ptr<Asset> asset) -> void {
  const auto typeIndex = asset->GetTypeIndex();
  const auto &id = asset->id;
  typeIndexToAssetSet[typeIndex].insert(id);
  idToAsset.insert_or_assign(id, std::move(asset));
  assetDb.SetStatus(id, AssetStatus::Loaded);
}
auto AssetManager::LoadMaterial(const aiMaterial &aiMaterial, SceneAsset &scene, const std::filesystem::path &path) -> int {
  const auto index = scene.materials.size();
  scene.materials.emplace_back();
  auto &material = scene.materials.back();
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
  if (aiMaterial.Get(AI_MATKEY_METALLIC_FACTOR, value) == AI_SUCCESS)
    material.fallback.metalness = value;
  else if (aiMaterial.Get(AI_MATKEY_REFLECTIVITY, value) == AI_SUCCESS)
    material.fallback.metalness = value;
  if (aiMaterial.Get(AI_MATKEY_ROUGHNESS_FACTOR, value) == AI_SUCCESS)
    material.fallback.roughness = value;
  else if (aiMaterial.Get(AI_MATKEY_SHININESS, value) == AI_SUCCESS)
    material.fallback.roughness = std::sqrt(2.f / (value + 2.f));
  if (!path.empty()) {
    LoadTexture(aiMaterial, aiTextureType_AMBIENT_OCCLUSION, material, scene, path);
    LoadTexture(aiMaterial, aiTextureType_DIFFUSE, material, scene, path);
    LoadTexture(aiMaterial, aiTextureType_DIFFUSE_ROUGHNESS, material, scene, path);
    LoadTexture(aiMaterial, aiTextureType_EMISSIVE, material, scene, path);
    LoadTexture(aiMaterial, aiTextureType_METALNESS, material, scene, path);
    LoadTexture(aiMaterial, aiTextureType_NORMALS, material, scene, path);
    LoadTexture(aiMaterial, aiTextureType_SPECULAR, material, scene, path);
  }
  spdlog::info("Loaded material: {}", path.string());
  return index;
}
auto AssetManager::LoadMesh(const aiMesh &aiMesh, SceneAsset &scene) -> int {
  const auto index = scene.meshes.size();
  scene.meshes.emplace_back();
  auto &mesh = scene.meshes.back();
  for (auto i = 0; i < aiMesh.mNumVertices; ++i) {
    mesh.vertices.emplace_back();
    auto &vertex = mesh.vertices.back();
    vertex.position = glm::vec3(aiMesh.mVertices[i].x, aiMesh.mVertices[i].y, aiMesh.mVertices[i].z);
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
      mesh.indices.push_back(face.mIndices[j]);
  }
  const auto vertexCount = mesh.vertices.size();
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
        if (mesh.vertices[vertexId].boneIds[k] < 0) {
          mesh.vertices[vertexId].boneIds[k] = boneId;
          mesh.vertices[vertexId].boneWeights[k] = weight;
          break;
        }
    }
  }
  return index;
}
auto AssetManager::LoadNode(const aiNode &aiNode, const aiScene &aiScene, SceneAsset &scene, const std::filesystem::path &path, int parent) -> int {
  const auto index = scene.nodes.size();
  scene.nodes.emplace_back();
  auto &node = scene.nodes.back();
  node.name = aiNode.mName.C_Str();
  node.transform = AssimpToGlmMat4(aiNode.mTransformation);
  node.parent = parent;
  for (auto i = 0; i < aiNode.mNumMeshes; ++i) {
    const auto meshId = aiNode.mMeshes[i];
    const auto aiMesh = aiScene.mMeshes[meshId];
    const auto meshIndex = LoadMesh(*aiMesh, scene);
    node.meshes.push_back(meshIndex);
    auto &mesh = scene.meshes.back();
    const auto matId = aiMesh->mMaterialIndex;
    if (matId >= 0 && matId < aiScene.mNumMaterials) {
      const auto aiMaterial = aiScene.mMaterials[matId];
      const auto materialIndex = LoadMaterial(*aiMaterial, scene, path);
      mesh.material = materialIndex;
    }
  }
  for (auto i = 0; i < aiNode.mNumChildren; ++i) {
    const auto childIndex = LoadNode(*aiNode.mChildren[i], aiScene, scene, path, index);
    node.children.push_back(childIndex);
  }
  return index;
}
auto AssetManager::LoadTexture(const aiMaterial &aiMaterial, const aiTextureType aiTextureType, SceneMaterial &material, SceneAsset &scene, const std::filesystem::path &path) -> void {
  const auto count = aiMaterial.GetTextureCount(aiTextureType);
  for (auto i = 0; i < count; ++i) {
    const auto index = scene.textures.size();
    SceneTexture texture{};
    texture.content = AssimpTexToContent(aiTextureType);
    aiString texPath;
    aiMaterial.GetTexture(aiTextureType, i, &texPath);
    const auto fullPath = (path / texPath.C_Str()).string();
    if (auto data = stbi_load(fullPath.c_str(), &texture.width, &texture.height, &texture.channels, 0); data) {
      const auto size = texture.width * texture.height * texture.channels;
      texture.data.assign(data, data + size);
      stbi_image_free(data);
      spdlog::info("Loaded texture: {}", path.string());
      material.textures.push_back(index);
      scene.textures.push_back(texture);
    }
  }
}
auto AssetManager::Unregister(const AssetID id) -> bool {
  assetDb.Unregister(id);
  return Unload(id);
}
} // namespace kuki
