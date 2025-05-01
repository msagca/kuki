#define GLM_ENABLE_EXPERIMENTAL
#define TINYEXR_IMPLEMENTATION
#include <application.hpp>
#include <array>
#include <asset_loader.hpp>
#include <assimp/config.h>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/matrix4x4.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <component/component.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/transform.hpp>
#include <cstdint>
#include <entity_manager.hpp>
#include <filesystem>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <primitive.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <string>
#include <thread>
#include <tinyexr.h>
#include <future>
#include <type_traits>
#include <vector>
namespace kuki {
AssetLoader::AssetLoader(Application* app, EntityManager& assetManager)
  : app(app), assetManager(assetManager) {}
void AssetLoader::Update() {
  while (auto eventOpt = textureLoadQueue.Pop())
    CreateTextureAsset(*eventOpt);
  while (auto eventOpt = cubeMapLoadQueue.Pop())
    CreateCubeMapAsset(*eventOpt);
  while (auto eventOpt = materialCreateQueue.Pop())
    (*eventOpt)->materialPromise.set_value(CreateMaterial((*eventOpt)->data));
  while (auto eventOpt = meshCreateQueue.Pop())
    (*eventOpt)->meshPromise.set_value(CreateMesh((*eventOpt)->source));
}
int AssetLoader::LoadPrimitive(PrimitiveId id) {
  int assetId = -1;
  switch (static_cast<uint8_t>(id)) {
  case static_cast<uint8_t>(PrimitiveId::Cube):
    assetId = LoadMesh("Cube", Primitive::Cube());
    break;
  case static_cast<uint8_t>(PrimitiveId::Sphere):
    assetId = LoadMesh("Sphere", Primitive::Sphere());
    break;
  case static_cast<uint8_t>(PrimitiveId::Cylinder):
    assetId = LoadMesh("Cylinder", Primitive::Cylinder());
    break;
  case static_cast<uint8_t>(PrimitiveId::Plane):
    assetId = LoadMesh("Plane", Primitive::Plane());
    break;
  case static_cast<uint8_t>(PrimitiveId::Frame):
    assetId = LoadMesh("Frame", Primitive::Frame());
    break;
  case static_cast<uint8_t>(PrimitiveId::CubeInverted): {
    auto vertices = Primitive::Cube();
    Primitive::FlipWindingOrder(vertices);
    assetId = LoadMesh("CubeInverted", vertices);
  } break;
  default:
    break;
  }
  assetManager.AddComponent<Transform>(assetId);
  assetManager.AddComponent<Material>(assetId);
  return assetId;
}
std::future<Material> AssetLoader::QueueMaterialCreation(const MaterialData& data) {
  auto pendingMaterial = new PendingMaterial{data};
  auto future = pendingMaterial->materialPromise.get_future();
  materialCreateQueue.Push(pendingMaterial);
  return future;
}
std::future<Mesh> AssetLoader::QueueMeshCreation(aiMesh* mesh) {
  auto pendingMesh = new PendingMesh{mesh, std::promise<Mesh>()};
  auto future = pendingMesh->meshPromise.get_future();
  meshCreateQueue.Push(pendingMesh);
  return future;
}
void AssetLoader::LoadModelAsync(const std::filesystem::path& path) {
  std::thread([this, path]() {
    if (!std::filesystem::exists(path)) {
      spdlog::error("File does not exist: '{}'.", path.string());
      return;
    }
    Assimp::Importer importer;
    const auto scene = importer.ReadFile(path.string(), aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);
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
    for (auto i = 0; i < materialFutures.size(); i++)
      materials.push_back(materialFutures[i].get());
    std::vector<std::future<Mesh>> meshFutures;
    for (auto i = 0; i < scene->mNumMeshes; ++i) {
      auto mesh = scene->mMeshes[i];
      meshFutures.push_back(QueueMeshCreation(mesh));
    }
    std::vector<Mesh> meshes;
    for (auto i = 0; i < meshFutures.size(); i++)
      meshes.push_back(meshFutures[i].get());
    LoadNode(scene->mRootNode, scene, path.parent_path(), materials, meshes, -1);
  }).detach();
}
int AssetLoader::LoadNode(const aiNode* aiNode, const aiScene* aiScene, const std::filesystem::path& path, const std::vector<Material>& materials, const std::vector<Mesh>& meshes, int parentId) {
  auto model = AssimpMatrix4x4ToGlmMat4(aiNode->mTransformation);
  std::string name = aiNode->mName.C_Str();
  // FIXME: perform entity creation and component addition in the main thread
  auto assetId = assetManager.Create(name);
  auto transform = assetManager.AddComponent<Transform>(assetId);
  if (parentId >= 0) {
    transform->parent = parentId;
    assetManager.AddChild(parentId, assetId);
  }
  glm::vec3 skew;
  glm::vec4 perspective;
  glm::decompose(model, transform->scale, transform->rotation, transform->position, skew, perspective);
  for (auto i = 0; i < aiNode->mNumMeshes; ++i) {
    auto meshIndex = aiNode->mMeshes[i];
    auto meshComp = assetManager.AddComponent<Mesh>(assetId);
    *meshComp = meshes[meshIndex];
    auto materialIndex = aiScene->mMeshes[meshIndex]->mMaterialIndex;
    if (materialIndex >= 0) {
      auto materialComp = assetManager.AddComponent<Material>(assetId);
      if (materialIndex >= 0 && materialIndex < materials.size())
        *materialComp = materials[materialIndex];
    }
  }
  for (auto i = 0; i < aiNode->mNumChildren; ++i)
    LoadNode(aiNode->mChildren[i], aiScene, path, materials, meshes, assetId);
  return assetId;
}
MaterialData AssetLoader::LoadMaterial(const aiMaterial* aiMaterial, const std::filesystem::path& root) {
  // TODO: the material does not always consist of textures, there could be color values as well
  aiString path;
  MaterialData data{};
  data.name = aiMaterial->GetName().C_Str();
  if (aiMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
    aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path);
    auto fullPath = root / path.C_Str();
    auto texture = LoadTexture(fullPath, TextureType::Albedo);
    if (texture.data)
      data.data[0] = texture;
  }
  if (aiMaterial->GetTextureCount(aiTextureType_NORMALS) > 0) {
    aiMaterial->GetTexture(aiTextureType_NORMALS, 0, &path);
    auto fullPath = root / path.C_Str();
    auto texture = LoadTexture(fullPath, TextureType::Normal);
    if (texture.data)
      data.data[1] = texture;
  }
  if (aiMaterial->GetTextureCount(aiTextureType_METALNESS) > 0) {
    aiMaterial->GetTexture(aiTextureType_METALNESS, 0, &path);
    auto fullPath = root / path.C_Str();
    auto texture = LoadTexture(fullPath, TextureType::Metalness);
    if (texture.data)
      data.data[2] = texture;
  }
  if (aiMaterial->GetTextureCount(aiTextureType_AMBIENT_OCCLUSION) > 0) {
    aiMaterial->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &path);
    auto fullPath = root / path.C_Str();
    auto texture = LoadTexture(fullPath, TextureType::Occlusion);
    if (texture.data)
      data.data[3] = texture;
  }
  if (aiMaterial->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0) {
    aiMaterial->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &path);
    auto fullPath = root / path.C_Str();
    auto texture = LoadTexture(fullPath, TextureType::Roughness);
    if (texture.data)
      data.data[4] = texture;
  }
  spdlog::info("Material is loaded: {}.", data.name);
  return data;
}
void AssetLoader::LoadTextureAsync(const std::filesystem::path& path, TextureType type) {
  std::thread([this, path, type]() {
    auto data = LoadTexture(path, type);
    if (data.data) {
      textureLoadQueue.Push(std::move(data));
      spdlog::info("Texture is loaded: '{}'.", path.string());
    }
  }).detach();
}
TextureData AssetLoader::LoadTexture(const std::filesystem::path& path, TextureType type) {
  TextureData data{};
  if (!std::filesystem::exists(path)) {
    spdlog::error("File does not exist: '{}'.", path.string());
    return data;
  }
  data.name = path.stem().string();
  data.type = type;
  auto ext = path.extension().string();
  auto pathStr = path.string();
  const char* pathCStr = pathStr.c_str();
  if (ext == ".exr") {
    data.type = TextureType::EXR;
    data.channels = 4;
    float* exrData = nullptr;
    const char* errMessage = nullptr;
    auto result = LoadEXR(&exrData, &data.width, &data.height, pathCStr, &errMessage);
    if (result != TINYEXR_SUCCESS) {
      if (errMessage) {
        spdlog::error("TinyEXR: ", errMessage);
        FreeEXRErrorMessage(errMessage);
      }
    } else
      data.data = reinterpret_cast<void*>(exrData);
  } else {
    auto bytesPerChannel = (type == TextureType::HDR) ? 4 : 1;
    if (bytesPerChannel > 2)
      data.data = stbi_loadf(pathCStr, &data.width, &data.height, &data.channels, 0);
    else if (bytesPerChannel > 1)
      data.data = stbi_load_16(pathCStr, &data.width, &data.height, &data.channels, 0);
    else
      data.data = stbi_load(pathCStr, &data.width, &data.height, &data.channels, 0);
    if (!data.data)
      spdlog::error("Failed to load texture: '{}'.", pathStr);
  }
  return data;
}
int AssetLoader::LoadCubeMap(const std::string& name, const std::array<std::filesystem::path, 6>& paths) {
  CubeMapData data;
  data.name = name;
  for (auto i = 0; i < 6; ++i) {
    auto& path = paths[i];
    auto texture = LoadTexture(path);
    if (texture.data)
      data.data[i] = texture;
    else {
      for (auto j = 0; j < i; ++j)
        stbi_image_free(data.data[j].data);
      return -1;
    }
  }
  return CreateCubeMapAsset(data);
}
void AssetLoader::LoadCubeMapAsync(const std::string& name, const std::array<std::filesystem::path, 6>& paths) {
  std::thread([this, name, paths]() {
    CubeMapData data;
    data.name = name;
    for (auto i = 0; i < 6; ++i) {
      auto& path = paths[i];
      auto texture = LoadTexture(path);
      if (texture.data)
        data.data[i] = texture;
      else {
        for (auto j = 0; j < i; ++j)
          stbi_image_free(data.data[j].data);
        return;
      }
    }
    cubeMapLoadQueue.Push(std::move(data));
    spdlog::info("Cube map is loaded: {}.", name);
  }).detach();
}
int AssetLoader::LoadMesh(const std::string& name, const std::vector<Vertex>& vertices) {
  std::string nameMutable = name;
  return LoadMesh(nameMutable, vertices);
}
int AssetLoader::LoadMesh(std::string& name, const std::vector<Vertex>& vertices) {
  auto assetId = assetManager.Create(name);
  auto mesh = assetManager.AddComponent<Mesh>(assetId);
  *mesh = CreateMesh(vertices);
  spdlog::info("Mesh is created: {}.", name);
  return assetId;
}
int AssetLoader::LoadMesh(std::string& name, const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
  auto assetId = assetManager.Create(name);
  auto mesh = assetManager.AddComponent<Mesh>(assetId);
  *mesh = CreateMesh(vertices, indices);
  spdlog::info("Mesh is created: {}.", name);
  return assetId;
}
glm::mat4 AssetLoader::AssimpMatrix4x4ToGlmMat4(const aiMatrix4x4& aiMatrix) {
  return {{aiMatrix.a1, aiMatrix.b1, aiMatrix.c1, aiMatrix.d1}, {aiMatrix.a2, aiMatrix.b2, aiMatrix.c2, aiMatrix.d2}, {aiMatrix.a3, aiMatrix.b3, aiMatrix.c3, aiMatrix.d3}, {aiMatrix.a4, aiMatrix.b4, aiMatrix.c4, aiMatrix.d4}};
}
} // namespace kuki
