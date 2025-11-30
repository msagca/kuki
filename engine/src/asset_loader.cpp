#define TINYEXR_IMPLEMENTATION
#include <application.hpp>
#include <array>
#include <asset_loader.hpp>
#include <assimp/color4.h>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/matrix4x4.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <bone_data.hpp>
#include <cmath>
#include <component.hpp>
#include <cstdint>
#include <entity_manager.hpp>
#include <filesystem>
#include <future>
#include <glm/detail/type_vec4.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <id.hpp>
#include <material.hpp>
#include <memory>
#include <mesh.hpp>
#include <primitive.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <string>
#include <thread>
#include <tinyexr.h>
#include <transform.hpp>
#include <utility>
#include <vector>
namespace kuki {
AssetLoader::AssetLoader(Application* app, EntityManager& assetManager)
  : app(app), assetManager(assetManager) {}
void AssetLoader::Update() {
  textureLoadQueue.Drain([this](std::unique_ptr<TextureData> textureData) { CreateTextureAsset(*textureData); });
  materialCreateQueue.Drain([this](std::unique_ptr<PendingMaterial> pendingMaterial) { pendingMaterial->materialPromise.set_value(CreateMaterial(pendingMaterial->materialData)); });
  meshCreateQueue.Drain([this](std::unique_ptr<PendingMesh> pendingMesh) { pendingMesh->meshPromise.set_value(CreateMesh(pendingMesh->meshPtr)); });
  boneDataCreateQueue.Drain([this](std::unique_ptr<PendingBoneData> pendingBoneData) { pendingBoneData->boneDataPromise.set_value(CreateBoneData(pendingBoneData->meshPtr)); });
  nodeCreateQueue.Drain([this](std::unique_ptr<PendingNode> pendingNode) { pendingNode->nodePromise.set_value(CreateNode(pendingNode->nodeData)); });
}
ID AssetLoader::LoadPrimitive(PrimitiveType id) {
  auto assetId = ID::Invalid();
  switch (static_cast<uint8_t>(id)) {
  case static_cast<uint8_t>(PrimitiveType::Cube):
    assetId = LoadMesh("Cube", Primitive::Cube());
    break;
  case static_cast<uint8_t>(PrimitiveType::Sphere):
    assetId = LoadMesh("Sphere", Primitive::Sphere());
    break;
  case static_cast<uint8_t>(PrimitiveType::Cylinder):
    assetId = LoadMesh("Cylinder", Primitive::Cylinder());
    break;
  case static_cast<uint8_t>(PrimitiveType::Plane):
    assetId = LoadMesh("Plane", Primitive::Plane());
    break;
  case static_cast<uint8_t>(PrimitiveType::Frame):
    assetId = LoadMesh("Frame", Primitive::Frame());
    break;
  case static_cast<uint8_t>(PrimitiveType::CubeInverted): {
    auto vertices = Primitive::Cube();
    Primitive::FlipWindingOrder(vertices);
    assetId = LoadMesh("CubeInverted", vertices);
  } break;
  default:
    break;
  }
  if (assetId.IsValid()) {
    assetManager.AddComponent<Transform>(assetId);
    assetManager.AddComponent<Material>(assetId);
  }
  return assetId;
}
std::future<ID> AssetLoader::QueueNodeCreation(const NodeData& data) {
  auto pendingNode = std::make_unique<PendingNode>(data);
  auto future = pendingNode->nodePromise.get_future();
  nodeCreateQueue.Push(std::move(pendingNode));
  return future;
}
std::future<Material> AssetLoader::QueueMaterialCreation(const MaterialData& data) {
  auto pendingMaterial = std::make_unique<PendingMaterial>(data);
  auto future = pendingMaterial->materialPromise.get_future();
  materialCreateQueue.Push(std::move(pendingMaterial));
  return future;
}
std::future<Mesh> AssetLoader::QueueMeshCreation(aiMesh* mesh) {
  auto pendingMesh = std::make_unique<PendingMesh>(mesh, std::promise<Mesh>());
  auto future = pendingMesh->meshPromise.get_future();
  meshCreateQueue.Push(std::move(pendingMesh));
  return future;
}
std::future<BoneData> AssetLoader::QueueBoneDataCreation(aiMesh* mesh) {
  auto pendingBoneData = std::make_unique<PendingBoneData>(mesh, std::promise<BoneData>());
  auto future = pendingBoneData->boneDataPromise.get_future();
  boneDataCreateQueue.Push(std::move(pendingBoneData));
  return future;
}
void AssetLoader::LoadModelAsync(std::filesystem::path path) {
  std::thread([this, path = std::move(path)]() {
    if (!std::filesystem::exists(path)) {
      spdlog::error("File does not exist: '{}'.", path.string());
      return;
    }
    Assimp::Importer importer;
    const auto scene = importer.ReadFile(path.string(), aiProcess_CalcTangentSpace | aiProcess_GlobalScale | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType | aiProcess_Triangulate);
    if (!scene || !scene->mRootNode) {
      spdlog::error("Assimp: {}", importer.GetErrorString());
      return;
    }
    auto root = path.parent_path();
    std::vector<std::future<Material>> materialFutures;
    for (auto i = 0; i < scene->mNumMaterials; ++i) {
      auto materialData = LoadMaterial(scene->mMaterials[i], root);
      materialFutures.push_back(QueueMaterialCreation(materialData));
    }
    std::vector<Material> materials;
    for (auto i = 0; i < materialFutures.size(); ++i)
      materials.push_back(materialFutures[i].get());
    std::vector<std::future<Mesh>> meshFutures;
    std::vector<std::future<BoneData>> boneDataFutures;
    for (auto i = 0; i < scene->mNumMeshes; ++i) {
      auto mesh = scene->mMeshes[i];
      meshFutures.push_back(QueueMeshCreation(mesh));
      boneDataFutures.push_back(QueueBoneDataCreation(mesh));
    }
    std::vector<Mesh> meshes;
    for (auto i = 0; i < meshFutures.size(); ++i)
      meshes.push_back(meshFutures[i].get());
    std::vector<BoneData> bones;
    for (auto i = 0; i < boneDataFutures.size(); ++i)
      bones.push_back(boneDataFutures[i].get());
    LoadNode(scene->mRootNode, scene, path.parent_path(), materials, meshes, bones);
  }).detach();
}
ID AssetLoader::LoadNode(const aiNode* aiNode, const aiScene* aiScene, const std::filesystem::path& path, const std::vector<Material>& materials, const std::vector<Mesh>& meshes, const std::vector<BoneData>& bones, const ID parentId) {
  NodeData nodeData{};
  nodeData.name = aiNode->mName.C_Str();
  nodeData.parent = parentId;
  nodeData.transform = AssimpToGlmMat4(aiNode->mTransformation);
  for (auto i = 0; i < aiNode->mNumMeshes; ++i) {
    auto meshIndex = aiNode->mMeshes[i];
    nodeData.mesh = meshes[meshIndex];
    nodeData.hasMesh = true;
    auto materialIndex = aiScene->mMeshes[meshIndex]->mMaterialIndex;
    if (materialIndex >= 0 && materialIndex < materials.size()) {
      nodeData.material = materials[materialIndex];
      nodeData.hasMaterial = true;
    }
    if (bones[meshIndex].boneSSBO > 0) {
      nodeData.boneData = bones[meshIndex];
      nodeData.hasBoneData = true;
    }
  }
  auto entityFuture = QueueNodeCreation(nodeData);
  auto assetId = entityFuture.get();
  for (auto i = 0; i < aiNode->mNumChildren; ++i)
    LoadNode(aiNode->mChildren[i], aiScene, path, materials, meshes, bones, assetId);
  return assetId;
}
MaterialData AssetLoader::LoadMaterial(const aiMaterial* aiMaterial, const std::filesystem::path& root) {
  MaterialData materialData{};
  materialData.name = aiMaterial->GetName().C_Str();
  aiColor4D color;
  float value;
  if (aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
    materialData.albedo = {color.r, color.g, color.b, color.a};
  if (aiMaterial->Get(AI_MATKEY_OPACITY, value) == AI_SUCCESS)
    materialData.albedo.w = value;
  if (aiMaterial->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
    materialData.specular = {color.r, color.g, color.b, color.a};
  if (aiMaterial->Get(AI_MATKEY_COLOR_AMBIENT, value) == AI_SUCCESS)
    materialData.occlusion = value;
  if (aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
    materialData.emissive = {color.r, color.g, color.b, color.a};
  if (aiMaterial->Get(AI_MATKEY_METALLIC_FACTOR, value) == AI_SUCCESS)
    materialData.metalness = value;
  else if (aiMaterial->Get(AI_MATKEY_REFLECTIVITY, value) == AI_SUCCESS)
    materialData.metalness = value;
  if (aiMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, value) == AI_SUCCESS)
    materialData.roughness = value;
  else if (aiMaterial->Get(AI_MATKEY_SHININESS, value) == AI_SUCCESS)
    materialData.roughness = std::sqrt(2.0f / (value + 2.0f));
  LoadTextureIfExists(aiMaterial, aiTextureType_DIFFUSE, root, materialData, 0, TextureType::Albedo);
  LoadTextureIfExists(aiMaterial, aiTextureType_NORMALS, root, materialData, 1, TextureType::Normal);
  LoadTextureIfExists(aiMaterial, aiTextureType_METALNESS, root, materialData, 2, TextureType::Metalness);
  LoadTextureIfExists(aiMaterial, aiTextureType_AMBIENT_OCCLUSION, root, materialData, 3, TextureType::Occlusion);
  LoadTextureIfExists(aiMaterial, aiTextureType_DIFFUSE_ROUGHNESS, root, materialData, 4, TextureType::Roughness);
  LoadTextureIfExists(aiMaterial, aiTextureType_SPECULAR, root, materialData, 5, TextureType::Specular);
  LoadTextureIfExists(aiMaterial, aiTextureType_EMISSIVE, root, materialData, 6, TextureType::Emissive);
  spdlog::info("Material loaded: {}.", materialData.name);
  return materialData;
}
void AssetLoader::LoadTextureIfExists(const aiMaterial* aiMaterial, aiTextureType textureType, const std::filesystem::path& root, MaterialData& materialData, int textureIndex, TextureType type) {
  if (aiMaterial->GetTextureCount(textureType) > 0) {
    aiString path;
    aiMaterial->GetTexture(textureType, 0, &path);
    auto fullPath = root / path.C_Str();
    auto texture = LoadTexture(fullPath, type);
    if (texture.data)
      materialData.textureData[textureIndex] = texture;
  }
  switch (textureType) {
  case aiTextureType_DIFFUSE:
    materialData.textureMask |= static_cast<int>(TextureMask::Albedo);
    break;
  case aiTextureType_NORMALS:
    materialData.textureMask |= static_cast<int>(TextureMask::Normal);
    break;
  case aiTextureType_METALNESS:
    materialData.textureMask |= static_cast<int>(TextureMask::Metalness);
    break;
  case aiTextureType_AMBIENT_OCCLUSION:
    materialData.textureMask |= static_cast<int>(TextureMask::Occlusion);
    break;
  case aiTextureType_DIFFUSE_ROUGHNESS:
    materialData.textureMask |= static_cast<int>(TextureMask::Roughness);
    break;
  case aiTextureType_SPECULAR:
    materialData.textureMask |= static_cast<int>(TextureMask::Specular);
    break;
  case aiTextureType_EMISSIVE:
    materialData.textureMask |= static_cast<int>(TextureMask::Emissive);
    break;
  default:
    break;
  }
}
void AssetLoader::LoadTextureAsync(std::filesystem::path path, TextureType type) {
  std::thread([this, path = std::move(path), type]() {
    auto textureData = LoadTexture(path, type);
    if (textureData.data) {
      textureLoadQueue.Push(std::make_unique<TextureData>(textureData));
      spdlog::info("Texture is loaded: '{}'.", path.string());
    }
  }).detach();
}
TextureData AssetLoader::LoadTexture(const std::filesystem::path& path, TextureType type) {
  TextureData textureData{};
  if (!std::filesystem::exists(path)) {
    spdlog::error("File does not exist: '{}'.", path.string());
    return textureData;
  }
  textureData.name = path.stem().string();
  textureData.type = type;
  auto ext = path.extension().string();
  auto pathStr = path.string();
  const char* pathCStr = pathStr.c_str();
  if (ext == ".exr") {
    textureData.type = TextureType::EXR;
    textureData.channels = 4;
    float* exrData = nullptr;
    const char* errMessage = nullptr;
    auto result = LoadEXR(&exrData, &textureData.width, &textureData.height, pathCStr, &errMessage);
    if (result != TINYEXR_SUCCESS) {
      if (errMessage) {
        spdlog::error("TinyEXR: ", errMessage);
        FreeEXRErrorMessage(errMessage);
      }
    } else
      textureData.data = reinterpret_cast<void*>(exrData);
  } else {
    auto bytesPerChannel = (type == TextureType::HDR) ? 4 : 1;
    if (bytesPerChannel > 2)
      textureData.data = stbi_loadf(pathCStr, &textureData.width, &textureData.height, &textureData.channels, 0);
    else if (bytesPerChannel > 1)
      textureData.data = stbi_load_16(pathCStr, &textureData.width, &textureData.height, &textureData.channels, 0);
    else
      textureData.data = stbi_load(pathCStr, &textureData.width, &textureData.height, &textureData.channels, 0);
    if (!textureData.data)
      spdlog::error("Failed to load texture: '{}'.", pathStr);
  }
  return textureData;
}
ID AssetLoader::LoadMesh(const std::string& name, const std::vector<Vertex>& vertices) {
  std::string nameMutable = name;
  return LoadMesh(nameMutable, vertices);
}
ID AssetLoader::LoadMesh(std::string& name, const std::vector<Vertex>& vertices) {
  auto assetId = assetManager.Create(name);
  auto mesh = assetManager.AddComponent<Mesh>(assetId);
  *mesh = CreateMesh(vertices);
  spdlog::info("Mesh is created: {}.", name);
  return assetId;
}
ID AssetLoader::LoadMesh(std::string& name, const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
  auto assetId = assetManager.Create(name);
  auto mesh = assetManager.AddComponent<Mesh>(assetId);
  *mesh = CreateMesh(vertices, indices);
  spdlog::info("Mesh is created: {}.", name);
  return assetId;
}
glm::mat4 AssetLoader::AssimpToGlmMat4(const aiMatrix4x4& m) {
  return glm::mat4(m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3, m.c3, m.d3, m.a4, m.b4, m.c4, m.d4);
}
} // namespace kuki
