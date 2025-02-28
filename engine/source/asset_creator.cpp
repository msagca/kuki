#include <asset_loader.hpp>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/types.h>
#include <component/component.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/texture.hpp>
#include <cstddef>
#include <entity_manager.hpp>
#include <filesystem>
#include <glad/glad.h>
#include <glm/detail/type_vec2.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <primitive.hpp>
#include <variant>
#include <vector>
Material AssetLoader::CreateMaterial(aiMaterial* aiMaterial, const std::filesystem::path& root) {
  Material material;
  material.material = PBRMaterial{};
  auto& pbrMaterial = std::get<PBRMaterial>(material.material);
  aiString path;
  if (aiMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
    aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path);
    auto fullPath = root / path.C_Str();
    auto id = LoadTexture(fullPath, TextureType::Base);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      pbrMaterial.base = texture->id;
  }
  if (aiMaterial->GetTextureCount(aiTextureType_NORMALS) > 0) {
    aiMaterial->GetTexture(aiTextureType_NORMALS, 0, &path);
    auto fullPath = root / path.C_Str();
    auto id = LoadTexture(fullPath, TextureType::Normal);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      pbrMaterial.normal = texture->id;
  }
  if (aiMaterial->GetTextureCount(aiTextureType_METALNESS) > 0) {
    aiMaterial->GetTexture(aiTextureType_METALNESS, 0, &path);
    auto fullPath = root / path.C_Str();
    auto id = LoadTexture(fullPath, TextureType::Metalness);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      pbrMaterial.metalness = texture->id;
  }
  if (aiMaterial->GetTextureCount(aiTextureType_AMBIENT_OCCLUSION) > 0) {
    aiMaterial->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &path);
    auto fullPath = root / path.C_Str();
    auto id = LoadTexture(fullPath, TextureType::Occlusion);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      pbrMaterial.occlusion = texture->id;
  }
  if (aiMaterial->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0) {
    aiMaterial->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &path);
    auto fullPath = root / path.C_Str();
    auto id = LoadTexture(fullPath, TextureType::Roughness);
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
      texCoord.y = 1.0f - aiMesh->mTextureCoords[0][i].y;
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
Mesh AssetLoader::CreateMesh(const std::vector<Vertex>& vertices) {
  auto mesh = CreateVertexBuffer(vertices);
  return mesh;
}
Mesh AssetLoader::CreateMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
  auto mesh = CreateMesh(vertices);
  CreateIndexBuffer(mesh, indices);
  return mesh;
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
