#define GLM_ENABLE_EXPERIMENTAL
#include <asset_loader.hpp>
#include <assimp/Importer.hpp>
#include <assimp/matrix4x4.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <component/component.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/shader.hpp>
#include <component/texture.hpp>
#include <component/transform.hpp>
#include <entity_manager.hpp>
#include <filesystem>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <iostream>
#include <primitive.hpp>
#include <stb_image.h>
#include <string>
#include <vector>
AssetLoader::AssetLoader(EntityManager& assetManager)
  : assetManager(assetManager) {}
int AssetLoader::LoadPrimitive(PrimitiveID id) {
  int assetID = -1;
  switch (static_cast<unsigned int>(id)) {
  case static_cast<unsigned int>(PrimitiveID::Cube):
    assetID = LoadMesh("Cube", Primitive::Cube());
    break;
  case static_cast<unsigned int>(PrimitiveID::Sphere):
    assetID = LoadMesh("Sphere", Primitive::Sphere());
    break;
  case static_cast<unsigned int>(PrimitiveID::Cylinder):
    assetID = LoadMesh("Cylinder", Primitive::Cylinder());
    break;
  case static_cast<unsigned int>(PrimitiveID::Plane):
    assetID = LoadMesh("Plane", Primitive::Plane());
    break;
  case static_cast<unsigned int>(PrimitiveID::Frame):
    assetID = LoadMesh("Frame", Primitive::Frame());
    break;
  case static_cast<unsigned int>(PrimitiveID::CubeInverted): {
    auto vertices = Primitive::Cube();
    Primitive::FlipWindingOrder(vertices);
    assetID = LoadMesh("CubeInverted", vertices);
  } break;
  default:
    break;
  }
  assetManager.AddComponent<Transform>(assetID);
  assetManager.AddComponent<Material>(assetID);
  return assetID;
}
int AssetLoader::LoadModel(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path))
    return -1;
  auto it = pathToID.find(path);
  if (it != pathToID.end())
    return it->second;
  Assimp::Importer importer;
  const auto scene = importer.ReadFile(path.string(), aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);
  if (!scene || !scene->mRootNode) {
    std::cerr << "Assimp: " << importer.GetErrorString() << std::endl;
    return -1;
  }
  auto assetID = LoadNode(scene->mRootNode, scene, path.parent_path(), -1);
  pathToID[path] = assetID;
  return assetID;
}
int AssetLoader::LoadNode(aiNode* aiNode, const aiScene* aiScene, const std::filesystem::path& path, int parentID) {
  auto model = AssimpMatrix4x4ToGlmMat4(aiNode->mTransformation);
  std::string name = aiNode->mName.C_Str();
  auto assetID = assetManager.Create(name);
  if (parentID >= 0)
    assetManager.AddChild(parentID, assetID);
  auto transform = assetManager.AddComponent<Transform>(assetID);
  transform->parent = parentID;
  glm::vec3 skew;
  glm::vec4 perspective;
  glm::decompose(model, transform->scale, transform->rotation, transform->position, skew, perspective);
  // TODO: re-check the scale normalization operations below
  transform->position /= transform->scale;
  transform->scale = glm::vec3(1.0f);
  for (auto i = 0; i < aiNode->mNumMeshes; ++i) {
    auto mesh = aiScene->mMeshes[aiNode->mMeshes[i]];
    auto meshComp = assetManager.AddComponent<Mesh>(assetID);
    *meshComp = CreateMesh(mesh);
    if (mesh->mMaterialIndex >= 0) {
      auto material = aiScene->mMaterials[mesh->mMaterialIndex];
      auto materialComp = assetManager.AddComponent<Material>(assetID);
      *materialComp = CreateMaterial(material, path);
    }
  }
  for (auto i = 0; i < aiNode->mNumChildren; ++i)
    LoadNode(aiNode->mChildren[i], aiScene, path, assetID);
  return assetID;
}
int AssetLoader::LoadTexture(const std::filesystem::path& path, TextureType type) {
  if (!std::filesystem::exists(path))
    return -1;
  auto it = pathToID.find(path);
  if (it != pathToID.end())
    return it->second;
  int width, height, nrComponents;
  auto data = stbi_load(path.string().c_str(), &width, &height, &nrComponents, 0);
  if (!data) {
    std::cerr << "Failed to load the texture: " << path << std::endl;
    return -1;
  }
  GLenum internalFormat, format;
  switch (nrComponents) {
  case 1:
    internalFormat = GL_R8;
    format = GL_RED;
    break;
  case 2:
    internalFormat = GL_RG8;
    format = GL_RG;
    break;
  case 3:
    internalFormat = (type == TextureType::Base) ? GL_SRGB8 : GL_RGB8;
    format = GL_RGB;
    break;
  case 4:
    internalFormat = (type == TextureType::Base) ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    format = GL_RGBA;
    break;
  default:
    stbi_image_free(data);
    std::cerr << "Unsupported texture format: " << format << std::endl;
    return -1;
  }
  unsigned int textureID;
  glCreateTextures(GL_TEXTURE_2D, 1, &textureID);
  auto levels = 1;
  if (type == TextureType::Base)
    levels = std::log2(std::max(width, height)) + 1;
  glTextureStorage2D(textureID, levels, internalFormat, width, height);
  glTextureSubImage2D(textureID, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, data);
  stbi_image_free(data);
  switch (type) {
  case TextureType::Base:
    glTextureParameteri(textureID, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(textureID, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(textureID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(textureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateTextureMipmap(textureID);
    break;
  case TextureType::Normal:
    glTextureParameteri(textureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(textureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    break;
  case TextureType::Roughness:
  case TextureType::Metalness:
  case TextureType::Occlusion:
    glTextureParameteri(textureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(textureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (nrComponents == 1) {
      glTextureParameteri(textureID, GL_TEXTURE_SWIZZLE_G, GL_RED);
      glTextureParameteri(textureID, GL_TEXTURE_SWIZZLE_B, GL_RED);
      glTextureParameteri(textureID, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    }
    break;
  default:
    std::cerr << "Unknown texture type: " << static_cast<int>(type) << std::endl;
    break;
  }
  auto name = path.stem().string();
  auto assetID = assetManager.Create(name);
  auto texture = assetManager.AddComponent<Texture>(assetID);
  texture->id = textureID;
  texture->type = type;
  pathToID[path] = assetID;
  return assetID;
}
bool AssetLoader::LoadCubeMapSide(unsigned int textureID, const std::filesystem::path& path, int side) {
  if (!std::filesystem::exists(path))
    return false;
  int width, height, nrComponents;
  auto data = stbi_load(path.string().c_str(), &width, &height, &nrComponents, 0);
  if (!data) {
    std::cerr << "Failed to load texture: " << path << std::endl;
    return false;
  }
  glTextureSubImage3D(textureID, 0, 0, 0, side - GL_TEXTURE_CUBE_MAP_POSITIVE_X, width, height, 1, GL_RGB, GL_UNSIGNED_BYTE, data);
  stbi_image_free(data);
  return true;
}
int AssetLoader::LoadCubeMap(std::string& name, const std::filesystem::path& top, const std::filesystem::path& bottom, const std::filesystem::path& right, const std::filesystem::path& left, const std::filesystem::path& front, const std::filesystem::path& back) {
  unsigned int textureID;
  glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &textureID);
  int width, height, channels;
  auto data = stbi_load(right.string().c_str(), &width, &height, &channels, 0);
  if (data) {
    glTextureStorage2D(textureID, 1, GL_RGB8, width, height);
    stbi_image_free(data);
  } else {
    std::cerr << "Failed to load texture for size determination" << std::endl;
    glDeleteTextures(1, &textureID);
    return -1;
  }
  LoadCubeMapSide(textureID, right, GL_TEXTURE_CUBE_MAP_POSITIVE_X);
  LoadCubeMapSide(textureID, left, GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
  LoadCubeMapSide(textureID, top, GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
  LoadCubeMapSide(textureID, bottom, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
  LoadCubeMapSide(textureID, front, GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
  LoadCubeMapSide(textureID, back, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);
  glTextureParameteri(textureID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTextureParameteri(textureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTextureParameteri(textureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTextureParameteri(textureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTextureParameteri(textureID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  auto assetID = assetManager.Create(name);
  auto texture = assetManager.AddComponent<Texture>(assetID);
  texture->id = textureID;
  texture->type = TextureType::CubeMap;
  return assetID;
}
int AssetLoader::LoadMesh(const std::string& name, const std::vector<Vertex>& vertices) {
  std::string nameMutable = name;
  return LoadMesh(nameMutable, vertices);
}
int AssetLoader::LoadMesh(std::string& name, const std::vector<Vertex>& vertices) {
  auto assetID = assetManager.Create(name);
  auto mesh = assetManager.AddComponent<Mesh>(assetID);
  *mesh = CreateMesh(vertices);
  return assetID;
}
int AssetLoader::LoadMesh(std::string& name, const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
  auto assetID = assetManager.Create(name);
  auto mesh = assetManager.AddComponent<Mesh>(assetID);
  *mesh = CreateMesh(vertices, indices);
  return assetID;
}
glm::mat4 AssetLoader::AssimpMatrix4x4ToGlmMat4(const aiMatrix4x4& aiMatrix) {
  return {{aiMatrix.a1, aiMatrix.b1, aiMatrix.c1, aiMatrix.d1}, {aiMatrix.a2, aiMatrix.b2, aiMatrix.c2, aiMatrix.d2}, {aiMatrix.a3, aiMatrix.b3, aiMatrix.c3, aiMatrix.d3}, {aiMatrix.a4, aiMatrix.b4, aiMatrix.c4, aiMatrix.d4}};
}
