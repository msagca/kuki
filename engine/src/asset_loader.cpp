#define GLM_ENABLE_EXPERIMENTAL
#define TINYEXR_IMPLEMENTATION
#include <algorithm>
#include <application.hpp>
#include <array>
#include <asset_loader.hpp>
#include <assimp/Importer.hpp>
#include <assimp/color4.h>
#include <assimp/material.h>
#include <assimp/matrix4x4.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <assimp/vector3.h>
#include <bone_data.hpp>
#include <cmath>
#include <component.hpp>
#include <cstddef>
#include <cstdint>
#include <entity_manager.hpp>
#include <filesystem>
#include <future>
#include <glad/glad.h>
#include <glm/detail/type_vec2.hpp>
#include <glm/detail/type_vec4.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/ext/vector_int4.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <id.hpp>
#include <limits>
#include <material.hpp>
#include <memory>
#include <mesh.hpp>
#include <primitive.hpp>
#include <skybox.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <string>
#include <texture.hpp>
#include <thread>
#include <tinyexr.h>
#include <transform.hpp>
#include <unordered_map>
#include <utility>
#include <variant>
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
    materialData.roughness = std::sqrt(2.f / (value + 2.f));
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
Texture AssetLoader::CreateTexture(const TextureData& textureData) {
  Texture texture{};
  if (!textureData.data) {
    spdlog::error("Failed to read texture data.");
    return texture;
  }
  GLenum internalFormat, format;
  auto isHDR = textureData.type == TextureType::HDR || textureData.type == TextureType::EXR;
  auto invalidChannels = false;
  switch (textureData.channels) {
  case 1:
    internalFormat = GL_R8;
    format = GL_RED;
    break;
  case 2:
    internalFormat = GL_RG8;
    format = GL_RG;
    break;
  case 3:
    if (isHDR)
      internalFormat = GL_RGB16F;
    else if (textureData.type == TextureType::Albedo)
      internalFormat = GL_SRGB8;
    else
      internalFormat = GL_RGB8;
    format = GL_RGB;
    break;
  case 4:
    if (isHDR)
      internalFormat = GL_RGBA16F;
    else if (textureData.type == TextureType::Albedo)
      internalFormat = GL_SRGB8_ALPHA8;
    else
      internalFormat = GL_RGBA8;
    format = GL_RGBA;
    break;
  default:
    invalidChannels = true;
    spdlog::error("Unsupported number of texture channels: {}.", textureData.channels);
    break;
  }
  unsigned int textureId;
  if (!invalidChannels) {
    glCreateTextures(GL_TEXTURE_2D, 1, &textureId);
    int levels = 1;
    if (textureData.type == TextureType::Albedo)
      levels = std::log2(std::max(textureData.width, textureData.height)) + 1;
    glTextureStorage2D(textureId, levels, internalFormat, textureData.width, textureData.height);
    glTextureSubImage2D(textureId, 0, 0, 0, textureData.width, textureData.height, format, isHDR ? GL_FLOAT : GL_UNSIGNED_BYTE, textureData.data);
  }
  stbi_image_free(textureData.data);
  if (invalidChannels)
    return texture;
  switch (textureData.type) {
  case TextureType::Albedo:
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateTextureMipmap(textureId);
    break;
  case TextureType::Normal:
  case TextureType::HDR:
  case TextureType::EXR:
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    break;
  case TextureType::Roughness:
  case TextureType::Metalness:
  case TextureType::Occlusion:
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    if (textureData.channels == 1) {
      glTextureParameteri(textureId, GL_TEXTURE_SWIZZLE_G, GL_RED);
      glTextureParameteri(textureId, GL_TEXTURE_SWIZZLE_B, GL_RED);
      glTextureParameteri(textureId, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    }
    break;
  default:
    spdlog::error("Unknown texture type: {}.", static_cast<int>(textureData.type));
    break;
  }
  texture.type = textureData.type;
  texture.width = textureData.width;
  texture.height = textureData.height;
  texture.id = textureId;
  return texture;
}
ID AssetLoader::CreateTextureAsset(const TextureData& textureData) {
  auto isHDR = textureData.type == TextureType::HDR || textureData.type == TextureType::EXR;
  if (isHDR)
    return CreateSkyboxAsset(textureData);
  auto texture = CreateTexture(textureData);
  auto name = textureData.name;
  auto assetId = assetManager.Create(name);
  auto textureComp = assetManager.AddComponent<Texture>(assetId);
  *textureComp = texture;
  spdlog::info("Texture is created: {}.", name);
  return assetId;
}
ID AssetLoader::CreateSkyboxAsset(const TextureData& textureData) {
  auto texture = CreateTexture(textureData);
  std::string name = textureData.name;
  auto assetId = app->CreateAsset(name);
  auto skybox = app->AddAssetComponent<Skybox>(assetId);
  auto cubeMap = app->CreateCubeMapFromEquirect(texture);
  skybox->original = cubeMap.id;
  skybox->irradiance = app->CreateIrradianceMapFromCubeMap(cubeMap).id;
  skybox->prefilter = app->CreatePrefilterMapFromCubeMap(cubeMap).id;
  skybox->brdf = app->CreateBRDF_LUT().id;
  spdlog::info("Skybox is created: {}.", name);
  return assetId;
}
Material AssetLoader::CreateMaterial(const MaterialData& materialData) {
  Material material{};
  material.current = LitMaterial{};
  auto litMaterial = std::get_if<LitMaterial>(&material.current);
  litMaterial->fallback.textureMask = materialData.textureMask;
  litMaterial->fallback.albedo = materialData.albedo;
  litMaterial->fallback.specular = materialData.specular;
  litMaterial->fallback.metalness = materialData.metalness;
  litMaterial->fallback.roughness = materialData.roughness;
  litMaterial->fallback.occlusion = materialData.occlusion;
  for (auto i = 0; i < materialData.textureData.size(); ++i) {
    if (!materialData.textureData[i].data)
      continue;
    auto assetId = CreateTextureAsset(materialData.textureData[i]);
    auto texture = assetManager.GetComponent<Texture>(assetId);
    if (!texture || texture->id == 0)
      continue;
    if (litMaterial) {
      switch (texture->type) {
      case TextureType::Albedo:
        litMaterial->data.albedo = texture->id;
        break;
      case TextureType::Normal:
        litMaterial->data.normal = texture->id;
        break;
      case TextureType::Metalness:
        litMaterial->data.metalness = texture->id;
        break;
      case TextureType::Occlusion:
        litMaterial->data.occlusion = texture->id;
        break;
      case TextureType::Roughness:
        litMaterial->data.roughness = texture->id;
        break;
      case TextureType::Specular:
        litMaterial->data.specular = texture->id;
        break;
      case TextureType::Emissive:
        litMaterial->data.emissive = texture->id;
        break;
      default:
        break;
      }
    }
  }
  return material;
}
ID AssetLoader::CreateMaterialAsset(const MaterialData& materialData) {
  auto name = materialData.name;
  auto assetId = assetManager.Create(name);
  auto material = assetManager.AddComponent<Material>(assetId);
  auto newMaterial = CreateMaterial(materialData);
  *material = newMaterial;
  return assetId;
}
Mesh AssetLoader::CreateMesh(const aiMesh* aiMesh) {
  std::vector<Vertex> vertices;
  for (auto i = 0; i < aiMesh->mNumVertices; ++i) {
    Vertex vertex{};
    vertex.position = glm::vec3(aiMesh->mVertices[i].x, aiMesh->mVertices[i].y, aiMesh->mVertices[i].z);
    vertex.normal = glm::vec3(aiMesh->mNormals[i].x, aiMesh->mNormals[i].y, aiMesh->mNormals[i].z);
    // FIXME: some models do not have tangents, and the following access results in an exception
    vertex.tangent = glm::vec3(aiMesh->mTangents[i].x, aiMesh->mTangents[i].y, aiMesh->mTangents[i].z);
    vertex.boneIds = glm::ivec4(-1); // NOTE: -1 indicates that this hasn't been assigned yet, will be used later
    if (aiMesh->mTextureCoords[0]) {
      glm::vec2 texCoord{};
      texCoord.x = aiMesh->mTextureCoords[0][i].x;
      texCoord.y = 1.f - aiMesh->mTextureCoords[0][i].y;
      vertex.texture = texCoord;
    } else
      vertex.texture = glm::vec2(0.f);
    vertices.push_back(vertex);
  }
  std::vector<unsigned int> indices;
  for (auto i = 0; i < aiMesh->mNumFaces; ++i) {
    auto& face = aiMesh->mFaces[i];
    for (auto j = 0; j < face.mNumIndices; ++j)
      indices.push_back(face.mIndices[j]);
  }
  auto vertexCount = vertices.size();
  std::unordered_map<std::string, unsigned int> boneNameToId;
  for (auto i = 0; i < aiMesh->mNumBones; ++i) {
    auto bone = aiMesh->mBones[i];
    auto boneName(bone->mName.C_Str());
    auto boneId = 0u;
    auto boneNameId = boneNameToId.find(boneName);
    if (boneNameId != boneNameToId.end())
      boneId = boneNameId->second;
    else {
      boneId = static_cast<unsigned int>(boneNameToId.size());
      boneNameToId[boneName] = boneId;
    }
    for (auto j = 0; j < bone->mNumWeights; ++j) {
      auto vertexId = bone->mWeights[j].mVertexId;
      if (vertexId < vertexCount)
        continue;
      auto weight = bone->mWeights[j].mWeight;
      for (auto k = 0; k < 4; ++k)
        // assign the weight to the next unassigned id
        // TODO: use the largest 4 weights by normalizing their sum to 1
        if (vertices[vertexId].boneIds[k] < 0) {
          vertices[vertexId].boneIds[k] = boneId;
          vertices[vertexId].boneWeights[k] = weight;
          break;
        }
    }
  }
  return CreateMesh(vertices, indices);
}
BoneData AssetLoader::CreateBoneData(const aiMesh* aiMesh) {
  std::unordered_map<std::string, unsigned int> boneNameToId;
  std::vector<glm::mat4> boneTransforms;
  for (auto i = 0; i < aiMesh->mNumBones; ++i) {
    auto bone = aiMesh->mBones[i];
    auto boneName(bone->mName.C_Str());
    auto boneId = 0u;
    auto boneNameId = boneNameToId.find(boneName);
    if (boneNameId != boneNameToId.end())
      boneId = boneNameId->second;
    else {
      boneId = static_cast<unsigned int>(boneNameToId.size());
      boneNameToId[boneName] = boneId;
      boneTransforms.push_back(AssimpToGlmMat4(bone->mOffsetMatrix));
    }
  }
  BoneData boneData;
  if (boneTransforms.size() > 0) {
    GLuint ssbo;
    glCreateBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(boneTransforms), boneTransforms.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    boneData.boneSSBO = ssbo;
    boneData.boneCount = boneNameToId.size();
  }
  return boneData;
}
ID AssetLoader::CreateNode(const NodeData& nodeData) {
  auto name = nodeData.name;
  const auto assetId = assetManager.Create(name);
  auto transform = assetManager.AddComponent<Transform>(assetId);
  glm::vec3 skew;
  glm::vec4 perspective;
  glm::decompose(nodeData.transform, transform->scale, transform->rotation, transform->position, skew, perspective);
  /*auto normalized = nodeData.transform * glm::scale(glm::vec3(1.f) / transform->scale);
  transform->scale = glm::vec3(1.f);
  transform->local = normalized;*/
  transform->local = nodeData.transform;
  assetManager.AddChild(nodeData.parent, assetId);
  if (nodeData.hasMesh) {
    auto mesh = assetManager.AddComponent<Mesh>(assetId);
    *mesh = nodeData.mesh;
  }
  if (nodeData.hasMaterial) {
    auto material = assetManager.AddComponent<Material>(assetId);
    *material = nodeData.material;
  }
  if (nodeData.hasBoneData) {
    auto boneData = assetManager.AddComponent<BoneData>(assetId);
    *boneData = nodeData.boneData;
  }
  return assetId;
}
Mesh AssetLoader::CreateMesh(const std::vector<Vertex>& vertices, bool skinned) {
  Mesh mesh;
  CreateVertexBuffer(mesh, vertices, skinned);
  CalculateBounds(mesh, vertices);
  return mesh;
}
Mesh AssetLoader::CreateMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, bool skinned) {
  auto mesh = CreateMesh(vertices, skinned);
  CreateIndexBuffer(mesh, indices);
  return mesh;
}
Mesh AssetLoader::CreateVertexBuffer(Mesh& mesh, const std::vector<Vertex>& vertices, bool skinned) {
  mesh.vertexCount = vertices.size();
  GLuint vao, vbo;
  glCreateVertexArrays(1, &vao);
  glCreateBuffers(1, &vbo);
  mesh.vao = vao;
  auto bindingIndex = 0;
  glNamedBufferData(vbo, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
  glVertexArrayVertexBuffer(mesh.vao, bindingIndex, vbo, 0, sizeof(Vertex));
  auto attribIndex = 0;
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, texture));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, tangent));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  if (skinned) {
    attribIndex++;
    glVertexArrayAttribFormat(mesh.vao, attribIndex, 4, GL_INT, GL_FALSE, offsetof(Vertex, boneIds));
    glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
    glEnableVertexArrayAttrib(mesh.vao, attribIndex);
    attribIndex++;
    glVertexArrayAttribFormat(mesh.vao, attribIndex, 4, GL_FLOAT, GL_FALSE, offsetof(Vertex, boneWeights));
    glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
    glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  }
  return mesh;
}
void AssetLoader::CreateIndexBuffer(Mesh& mesh, const std::vector<unsigned int>& indices) {
  mesh.indexCount = indices.size();
  GLuint indexBuffer;
  glCreateBuffers(1, &indexBuffer);
  mesh.ebo = indexBuffer;
  glNamedBufferData(mesh.ebo, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
  glVertexArrayElementBuffer(mesh.vao, mesh.ebo);
}
void AssetLoader::CalculateBounds(Mesh& mesh, const std::vector<Vertex>& vertices) {
  mesh.bounds.min = glm::vec3(std::numeric_limits<float>::max());
  mesh.bounds.max = glm::vec3(std::numeric_limits<float>::lowest());
  for (const auto& vertex : vertices) {
    mesh.bounds.min = glm::min(mesh.bounds.min, vertex.position);
    mesh.bounds.max = glm::max(mesh.bounds.max, vertex.position);
  }
}
} // namespace kuki
