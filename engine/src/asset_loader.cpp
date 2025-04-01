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
int AssetLoader::LoadPrimitive(PrimitiveId id) {
  int assetId = -1;
  switch (static_cast<unsigned int>(id)) {
  case static_cast<unsigned int>(PrimitiveId::Cube):
    assetId = LoadMesh("Cube", Primitive::Cube());
    break;
  case static_cast<unsigned int>(PrimitiveId::Sphere):
    assetId = LoadMesh("Sphere", Primitive::Sphere());
    break;
  case static_cast<unsigned int>(PrimitiveId::Cylinder):
    assetId = LoadMesh("Cylinder", Primitive::Cylinder());
    break;
  case static_cast<unsigned int>(PrimitiveId::Plane):
    assetId = LoadMesh("Plane", Primitive::Plane());
    break;
  case static_cast<unsigned int>(PrimitiveId::Frame):
    assetId = LoadMesh("Frame", Primitive::Frame());
    break;
  case static_cast<unsigned int>(PrimitiveId::CubeInverted): {
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
int AssetLoader::LoadModel(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path))
    return -1;
  auto it = pathToId.find(path);
  if (it != pathToId.end())
    return it->second;
  Assimp::Importer importer;
  const auto scene = importer.ReadFile(path.string(), aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);
  if (!scene || !scene->mRootNode) {
    std::cerr << "Assimp: " << importer.GetErrorString() << std::endl;
    return -1;
  }
  auto assetId = LoadNode(scene->mRootNode, scene, path.parent_path(), -1);
  pathToId[path] = assetId;
  return assetId;
}
int AssetLoader::LoadNode(aiNode* aiNode, const aiScene* aiScene, const std::filesystem::path& path, int parentId) {
  auto model = AssimpMatrix4x4ToGlmMat4(aiNode->mTransformation);
  std::string name = aiNode->mName.C_Str();
  auto assetId = assetManager.Create(name);
  if (parentId >= 0)
    assetManager.AddChild(parentId, assetId);
  auto transform = assetManager.AddComponent<Transform>(assetId);
  transform->parent = parentId;
  glm::vec3 skew;
  glm::vec4 perspective;
  glm::decompose(model, transform->scale, transform->rotation, transform->position, skew, perspective);
  // TODO: re-check the scale normalization operations below
  transform->position /= transform->scale;
  transform->scale = glm::vec3(1.0f);
  for (auto i = 0; i < aiNode->mNumMeshes; ++i) {
    auto mesh = aiScene->mMeshes[aiNode->mMeshes[i]];
    auto meshComp = assetManager.AddComponent<Mesh>(assetId);
    *meshComp = CreateMesh(mesh);
    if (mesh->mMaterialIndex >= 0) {
      auto material = aiScene->mMaterials[mesh->mMaterialIndex];
      auto materialComp = assetManager.AddComponent<Material>(assetId);
      *materialComp = CreateMaterial(material, path);
    }
  }
  for (auto i = 0; i < aiNode->mNumChildren; ++i)
    LoadNode(aiNode->mChildren[i], aiScene, path, assetId);
  return assetId;
}
int AssetLoader::LoadTexture(const std::filesystem::path& path, TextureType type) {
  if (!std::filesystem::exists(path))
    return -1;
  auto it = pathToId.find(path);
  if (it != pathToId.end())
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
    std::cerr << "Unsupported number of texture channels: " << nrComponents << std::endl;
    return -1;
  }
  unsigned int textureId;
  glCreateTextures(GL_TEXTURE_2D, 1, &textureId);
  auto levels = 1;
  if (type == TextureType::Base)
    levels = std::log2(std::max(width, height)) + 1;
  glTextureStorage2D(textureId, levels, internalFormat, width, height);
  glTextureSubImage2D(textureId, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, data);
  stbi_image_free(data);
  switch (type) {
  case TextureType::Base:
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(textureId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateTextureMipmap(textureId);
    break;
  case TextureType::Normal:
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(textureId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    break;
  case TextureType::Roughness:
  case TextureType::Metalness:
  case TextureType::Occlusion:
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(textureId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (nrComponents == 1) {
      glTextureParameteri(textureId, GL_TEXTURE_SWIZZLE_G, GL_RED);
      glTextureParameteri(textureId, GL_TEXTURE_SWIZZLE_B, GL_RED);
      glTextureParameteri(textureId, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    }
    break;
  default:
    std::cerr << "Unknown texture type: " << static_cast<int>(type) << std::endl;
    break;
  }
  auto name = path.stem().string();
  auto assetId = assetManager.Create(name);
  auto texture = assetManager.AddComponent<Texture>(assetId);
  texture->id = textureId;
  texture->type = type;
  pathToId[path] = assetId;
  return assetId;
}
bool AssetLoader::LoadCubeMapSide(unsigned int textureId, const std::filesystem::path& path, int side) {
  if (!std::filesystem::exists(path))
    return false;
  int width, height, nrComponents;
  auto data = stbi_load(path.string().c_str(), &width, &height, &nrComponents, 0);
  if (!data) {
    std::cerr << "Failed to load texture: " << path << std::endl;
    return false;
  }
  glTextureSubImage3D(textureId, 0, 0, 0, side - GL_TEXTURE_CUBE_MAP_POSITIVE_X, width, height, 1, GL_RGB, GL_UNSIGNED_BYTE, data);
  stbi_image_free(data);
  return true;
}
int AssetLoader::LoadCubeMap(std::string& name, const std::filesystem::path& top, const std::filesystem::path& bottom, const std::filesystem::path& right, const std::filesystem::path& left, const std::filesystem::path& front, const std::filesystem::path& back) {
  unsigned int textureId;
  glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &textureId);
  int width, height, channels;
  auto data = stbi_load(top.string().c_str(), &width, &height, &channels, 0);
  if (data) {
    glTextureStorage2D(textureId, 1, GL_RGB8, width, height);
    stbi_image_free(data);
  } else {
    std::cerr << "Failed to load texture: " << top << std::endl;
    glDeleteTextures(1, &textureId);
    return -1;
  }
  LoadCubeMapSide(textureId, right, GL_TEXTURE_CUBE_MAP_POSITIVE_X);
  LoadCubeMapSide(textureId, left, GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
  LoadCubeMapSide(textureId, top, GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
  LoadCubeMapSide(textureId, bottom, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
  LoadCubeMapSide(textureId, front, GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
  LoadCubeMapSide(textureId, back, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);
  glTextureParameteri(textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTextureParameteri(textureId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTextureParameteri(textureId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTextureParameteri(textureId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTextureParameteri(textureId, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  auto assetId = assetManager.Create(name);
  auto texture = assetManager.AddComponent<Texture>(assetId);
  texture->id = textureId;
  texture->type = TextureType::CubeMap;
  return assetId;
}
int AssetLoader::LoadMesh(const std::string& name, const std::vector<Vertex>& vertices) {
  std::string nameMutable = name;
  return LoadMesh(nameMutable, vertices);
}
int AssetLoader::LoadMesh(std::string& name, const std::vector<Vertex>& vertices) {
  auto assetId = assetManager.Create(name);
  auto mesh = assetManager.AddComponent<Mesh>(assetId);
  *mesh = CreateMesh(vertices);
  return assetId;
}
int AssetLoader::LoadMesh(std::string& name, const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
  auto assetId = assetManager.Create(name);
  auto mesh = assetManager.AddComponent<Mesh>(assetId);
  *mesh = CreateMesh(vertices, indices);
  return assetId;
}
glm::mat4 AssetLoader::AssimpMatrix4x4ToGlmMat4(const aiMatrix4x4& aiMatrix) {
  return {{aiMatrix.a1, aiMatrix.b1, aiMatrix.c1, aiMatrix.d1}, {aiMatrix.a2, aiMatrix.b2, aiMatrix.c2, aiMatrix.d2}, {aiMatrix.a3, aiMatrix.b3, aiMatrix.c3, aiMatrix.d3}, {aiMatrix.a4, aiMatrix.b4, aiMatrix.c4, aiMatrix.d4}};
}
