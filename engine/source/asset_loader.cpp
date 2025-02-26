#define GLM_ENABLE_EXPERIMENTAL
#define STB_IMAGE_IMPLEMENTATION
#include <asset_loader.hpp>
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
#include <component/shader.hpp>
#include <component/texture.hpp>
#include <component/transform.hpp>
#include <cstddef>
#include <entity_manager.hpp>
#include <filesystem>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <iostream>
#include <primitive.hpp>
#include <stb_image.h>
#include <string>
#include <variant>
#include <vector>
AssetLoader::AssetLoader(EntityManager& assetManager)
  : assetManager(assetManager) {}
int AssetLoader::LoadMaterial(std::string& name, const std::filesystem::path& basePath, const std::filesystem::path& normalPath, const std::filesystem::path& metalnessPath, const std::filesystem::path& occlusionPath, const std::filesystem::path& roughnessPath) {
  auto fullName = name + "Base";
  auto textureID = LoadTexture(fullName, basePath, TextureType::Base);
  auto texture = assetManager.GetComponent<Texture>(textureID);
  if (!texture)
    return -1;
  auto assetID = assetManager.Create(name);
  auto material = assetManager.AddComponent<Material>(assetID);
  material->material = PBRMaterial{};
  auto& pbrMat = std::get<PBRMaterial>(material->material);
  pbrMat.base = texture->id;
  if (!normalPath.empty()) {
    fullName = name + "Normal";
    textureID = LoadTexture(fullName, normalPath, TextureType::Normal);
    texture = assetManager.GetComponent<Texture>(textureID);
    if (texture)
      pbrMat.normal = texture->id;
  }
  if (!metalnessPath.empty()) {
    fullName = name + "Metalness";
    textureID = LoadTexture(fullName, metalnessPath, TextureType::Metalness);
    texture = assetManager.GetComponent<Texture>(textureID);
    if (texture)
      pbrMat.metalness = texture->id;
  }
  if (!occlusionPath.empty()) {
    fullName = name + "Occlusion";
    textureID = LoadTexture(fullName, occlusionPath, TextureType::Occlusion);
    texture = assetManager.GetComponent<Texture>(textureID);
    if (texture)
      pbrMat.occlusion = texture->id;
  }
  if (!roughnessPath.empty()) {
    fullName = name + "Roughness";
    textureID = LoadTexture(fullName, roughnessPath, TextureType::Roughness);
    texture = assetManager.GetComponent<Texture>(textureID);
    if (texture)
      pbrMat.roughness = texture->id;
  }
  return assetID;
}
int AssetLoader::LoadTexture(std::string& name, const std::filesystem::path& path, TextureType type) {
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
  unsigned int id;
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);
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
  auto assetID = assetManager.Create(name);
  auto texture = assetManager.AddComponent<Texture>(assetID);
  texture->id = id;
  texture->type = type;
  pathToID[path] = assetID;
  return assetID;
}
Material AssetLoader::CreateMaterial(aiMaterial* aiMaterial, std::string& name, const std::filesystem::path& root) {
  Material material;
  material.material = PBRMaterial{};
  auto& pbrMaterial = std::get<PBRMaterial>(material.material);
  aiString path;
  if (aiMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
    aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path);
    auto fullPath = root / path.C_Str();
    auto fullName = name + "Base";
    auto id = LoadTexture(fullName, fullPath, TextureType::Base);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      pbrMaterial.base = texture->id;
  }
  if (aiMaterial->GetTextureCount(aiTextureType_NORMALS) > 0) {
    aiMaterial->GetTexture(aiTextureType_NORMALS, 0, &path);
    auto fullPath = root / path.C_Str();
    auto fullName = name + "Normal";
    auto id = LoadTexture(fullName, fullPath, TextureType::Normal);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      pbrMaterial.normal = texture->id;
  }
  if (aiMaterial->GetTextureCount(aiTextureType_METALNESS) > 0) {
    aiMaterial->GetTexture(aiTextureType_METALNESS, 0, &path);
    auto fullPath = root / path.C_Str();
    auto fullName = name + "Metalness";
    auto id = LoadTexture(fullName, fullPath, TextureType::Metalness);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      pbrMaterial.metalness = texture->id;
  }
  if (aiMaterial->GetTextureCount(aiTextureType_AMBIENT_OCCLUSION) > 0) {
    aiMaterial->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &path);
    auto fullPath = root / path.C_Str();
    auto fullName = name + "Occlusion";
    auto id = LoadTexture(fullName, fullPath, TextureType::Occlusion);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      pbrMaterial.occlusion = texture->id;
  }
  if (aiMaterial->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0) {
    aiMaterial->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &path);
    auto fullPath = root / path.C_Str();
    auto fullName = name + "Roughness";
    auto id = LoadTexture(fullName, fullPath, TextureType::Roughness);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      pbrMaterial.roughness = texture->id;
  }
  return material;
}
Mesh AssetLoader::CreateMesh(aiMesh* aiMesh) {
  std::vector<Vertex> vertices;
  for (auto i = 0; i < aiMesh->mNumVertices; ++i) {
    Vertex vertex{};
    glm::vec3 vector{};
    vector.x = aiMesh->mVertices[i].x;
    vector.y = aiMesh->mVertices[i].y;
    vector.z = aiMesh->mVertices[i].z;
    vertex.position = vector;
    vector.x = aiMesh->mNormals[i].x;
    vector.y = aiMesh->mNormals[i].y;
    vector.z = aiMesh->mNormals[i].z;
    vertex.normal = vector;
    if (aiMesh->mTextureCoords[0]) {
      glm::vec2 texCoord{};
      texCoord.x = aiMesh->mTextureCoords[0][i].x;
      texCoord.y = aiMesh->mTextureCoords[0][i].y;
      vertex.texture = texCoord;
    } else
      vertex.texture = glm::vec2(.0f);
    vertices.push_back(vertex);
  }
  std::vector<unsigned int> indices;
  for (auto i = 0; i < aiMesh->mNumFaces; ++i) {
    auto& face = aiMesh->mFaces[i];
    for (auto j = 0; j < face.mNumIndices; ++j)
      indices.push_back(face.mIndices[j]);
  }
  return CreateMesh(vertices, indices);
}
glm::mat4 AssetLoader::AssimpMatrix4x4ToGlmMat4(const aiMatrix4x4& aiMatrix) {
  return {{aiMatrix.a1, aiMatrix.b1, aiMatrix.c1, aiMatrix.d1}, {aiMatrix.a2, aiMatrix.b2, aiMatrix.c2, aiMatrix.d2}, {aiMatrix.a3, aiMatrix.b3, aiMatrix.c3, aiMatrix.d3}, {aiMatrix.a4, aiMatrix.b4, aiMatrix.c4, aiMatrix.d4}};
}
int AssetLoader::LoadNode(aiNode* aiNode, const aiScene* aiScene, const std::filesystem::path& path, int parentID, const std::string& nodeName) {
  auto model = AssimpMatrix4x4ToGlmMat4(aiNode->mTransformation);
  auto name = nodeName.empty() ? aiNode->mName.C_Str() : nodeName;
  auto assetID = assetManager.Create(name);
  auto transform = assetManager.AddComponent<Transform>(assetID);
  glm::vec3 skew;
  glm::vec4 perspective;
  glm::quat orientation;
  glm::decompose(model, transform->scale, orientation, transform->position, skew, perspective);
  transform->position /= transform->scale;
  transform->scale = glm::vec3(1.0f);
  transform->rotation = glm::eulerAngles(orientation);
  transform->parent = parentID;
  for (auto i = 0; i < aiNode->mNumMeshes; ++i) {
    auto mesh = aiScene->mMeshes[aiNode->mMeshes[i]];
    auto meshComp = assetManager.AddComponent<Mesh>(assetID);
    *meshComp = CreateMesh(mesh);
    if (mesh->mMaterialIndex >= 0) {
      auto material = aiScene->mMaterials[mesh->mMaterialIndex];
      auto materialComp = assetManager.AddComponent<Material>(assetID);
      *materialComp = CreateMaterial(material, name, path);
    }
  }
  for (auto i = 0; i < aiNode->mNumChildren; ++i) {
    auto childID = LoadNode(aiNode->mChildren[i], aiScene, path, assetID);
    assetManager.AddChild(assetID, childID);
  }
  return assetID;
}
int AssetLoader::LoadModel(std::string& name, const std::filesystem::path& path) {
  auto it = pathToID.find(path);
  if (it != pathToID.end())
    return it->second;
  Assimp::Importer importer;
  const auto scene = importer.ReadFile(path.string(), aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);
  if (!scene || !scene->mRootNode) {
    std::cerr << "Assimp: " << importer.GetErrorString() << std::endl;
    return -1;
  }
  auto assetID = LoadNode(scene->mRootNode, scene, path.parent_path(), -1, name);
  pathToID[path] = assetID;
  return assetID;
}
Mesh AssetLoader::CreateVertexBuffer(const std::vector<Vertex>& vertices) {
  Mesh mesh;
  mesh.vertexCount = vertices.size();
  glGenVertexArrays(1, &mesh.vertexArray);
  glBindVertexArray(mesh.vertexArray);
  glGenBuffers(1, &mesh.vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texture));
  glEnableVertexAttribArray(2);
  return mesh;
}
void AssetLoader::CreateIndexBuffer(Mesh& mesh, const std::vector<unsigned int>& indices) {
  mesh.indexCount = indices.size();
  glBindVertexArray(mesh.vertexArray);
  glGenBuffers(1, &mesh.indexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
}
Mesh AssetLoader::CreateMesh(const std::vector<Vertex>& vertices) {
  auto mesh = CreateVertexBuffer(vertices);
  return mesh;
}
Mesh AssetLoader::CreateMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
  auto mesh = CreateMesh(vertices);
  CreateIndexBuffer(mesh, indices);
  return mesh;
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
