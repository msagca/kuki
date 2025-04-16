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
#include <limits>
#include <primitive.hpp>
#include <variant>
#include <vector>
namespace kuki {
Material AssetLoader::CreateMaterial(aiMaterial* aiMaterial, const std::filesystem::path& root) {
  Material material;
  material.material = LitMaterial{};
  auto& pbrMaterial = std::get<LitMaterial>(material.material);
  aiString path;
  if (aiMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
    aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path);
    auto fullPath = root / path.C_Str();
    auto id = LoadTexture(fullPath, TextureType::Albedo);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      pbrMaterial.albedo = *texture;
  }
  if (aiMaterial->GetTextureCount(aiTextureType_NORMALS) > 0) {
    aiMaterial->GetTexture(aiTextureType_NORMALS, 0, &path);
    auto fullPath = root / path.C_Str();
    auto id = LoadTexture(fullPath, TextureType::Normal);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      pbrMaterial.normal = *texture;
  }
  if (aiMaterial->GetTextureCount(aiTextureType_METALNESS) > 0) {
    aiMaterial->GetTexture(aiTextureType_METALNESS, 0, &path);
    auto fullPath = root / path.C_Str();
    auto id = LoadTexture(fullPath, TextureType::Metalness);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      pbrMaterial.metalness = *texture;
  }
  if (aiMaterial->GetTextureCount(aiTextureType_AMBIENT_OCCLUSION) > 0) {
    aiMaterial->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &path);
    auto fullPath = root / path.C_Str();
    auto id = LoadTexture(fullPath, TextureType::Occlusion);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      pbrMaterial.occlusion = *texture;
  }
  if (aiMaterial->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0) {
    aiMaterial->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &path);
    auto fullPath = root / path.C_Str();
    auto id = LoadTexture(fullPath, TextureType::Roughness);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      pbrMaterial.roughness = *texture;
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
    vector.x = aiMesh->mTangents[i].x;
    vector.y = aiMesh->mTangents[i].y;
    vector.z = aiMesh->mTangents[i].z;
    vertex.tangent = vector;
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
  CalculateBounds(mesh, vertices);
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
  unsigned int vertexArray, vertexBuffer;
  glCreateVertexArrays(1, &vertexArray);
  glCreateBuffers(1, &vertexBuffer);
  mesh.vertexArray = vertexArray;
  mesh.vertexBuffer = vertexBuffer;
  auto bindingIndex = 0;
  glNamedBufferData(mesh.vertexBuffer, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
  glVertexArrayVertexBuffer(mesh.vertexArray, 0, mesh.vertexBuffer, 0, sizeof(Vertex));
  glVertexArrayAttribFormat(mesh.vertexArray, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
  glVertexArrayAttribBinding(mesh.vertexArray, 0, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vertexArray, 0);
  glVertexArrayAttribFormat(mesh.vertexArray, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
  glVertexArrayAttribBinding(mesh.vertexArray, 1, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vertexArray, 1);
  glVertexArrayAttribFormat(mesh.vertexArray, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, texture));
  glVertexArrayAttribBinding(mesh.vertexArray, 2, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vertexArray, 2);
  glVertexArrayAttribFormat(mesh.vertexArray, 3, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, tangent));
  glVertexArrayAttribBinding(mesh.vertexArray, 3, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vertexArray, 3);
  return mesh;
}
void AssetLoader::CreateIndexBuffer(Mesh& mesh, const std::vector<unsigned int>& indices) {
  mesh.indexCount = indices.size();
  unsigned int indexBuffer;
  glCreateBuffers(1, &indexBuffer);
  mesh.indexBuffer = indexBuffer;
  glNamedBufferData(mesh.indexBuffer, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
  glVertexArrayElementBuffer(mesh.vertexArray, mesh.indexBuffer);
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
