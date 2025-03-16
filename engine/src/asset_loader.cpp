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
  transform->position /= transform->scale;
  transform->scale = glm::vec3(1.0f);
  // NOTE: is there a better way to normalize the scale?
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
    std::cerr << "Failed to load the texture at " << path << "." << std::endl;
    return -1;
  }
  GLint internalFormat;
  GLenum format;
  switch (nrComponents) {
  case 1:
    internalFormat = GL_R8;
    format = GL_RED;
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
    std::cerr << "Unexpected number of components in texture (" << nrComponents << ")." << std::endl;
    stbi_image_free(data);
    return -1;
  }
  unsigned int textureID;
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);
  glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
  switch (type) {
  case TextureType::Normal:
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    break;
  default:
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }
  stbi_image_free(data);
  auto name = path.stem().string();
  auto assetID = assetManager.Create(name);
  auto texture = assetManager.AddComponent<Texture>(assetID);
  texture->id = textureID;
  texture->type = type;
  pathToID[path] = assetID;
  return assetID;
}
bool AssetLoader::LoadCubeMapSide(const std::filesystem::path& path, int side) {
  if (!std::filesystem::exists(path))
    return false;
  int width, height, nrComponents;
  auto data = stbi_load(path.string().c_str(), &width, &height, &nrComponents, 0);
  if (!data) {
    std::cerr << "Failed to load the texture at " << path << "." << std::endl;
    return false;
  }
  glTexImage2D(side, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
  return true;
}
int AssetLoader::LoadCubeMap(std::string& name, const std::filesystem::path& top, const std::filesystem::path& bottom, const std::filesystem::path& right, const std::filesystem::path& left, const std::filesystem::path& front, const std::filesystem::path& back) {
  unsigned int textureID;
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
  LoadCubeMapSide(top, GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
  LoadCubeMapSide(bottom, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
  LoadCubeMapSide(right, GL_TEXTURE_CUBE_MAP_POSITIVE_X);
  LoadCubeMapSide(left, GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
  LoadCubeMapSide(front, GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
  LoadCubeMapSide(back, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
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
