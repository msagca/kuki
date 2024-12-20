#include <asset_manager.hpp>
#include <glad/glad.h>
#include <unordered_map>
#include <string>
#include <vector>
AssetManager::~AssetManager() {
  for (const auto& [_, mesh] : nameMeshMap) {
    glDeleteBuffers(1, &mesh.vertexBuffer);
    glDeleteBuffers(1, &mesh.indexBuffer);
    glDeleteVertexArrays(1, &mesh.vertexArray);
  }
}
Mesh AssetManager::CreateMesh(const std::string& name, const std::vector<float>& vertices) {
  auto hash = ComputeMeshHash(vertices);
  if (meshHashNameMap.find(hash) != meshHashNameMap.end())
    return nameMeshMap[meshHashNameMap[hash]];
  auto mesh = CreateVertexBuffer(vertices, true);
  nameMeshMap[name] = mesh;
  meshHashNameMap[hash] = name;
  meshNameHashMap[name] = hash;
  meshIndexNameMap[mesh.vertexArray] = name;
  return mesh;
}
const Mesh& AssetManager::GetMesh(const std::string& name) const {
  auto it = nameMeshMap.find(name);
  if (it != nameMeshMap.end())
    return it->second;
  static Mesh defaultMesh{};
  return defaultMesh;
}
const std::string& AssetManager::GetMeshName(unsigned int vao) const {
  auto it = meshIndexNameMap.find(vao);
  if (it != meshIndexNameMap.end())
    return it->second;
  static std::string defaultName{};
  return defaultName;
}
void AssetManager::DeleteMesh(const std::string& name) {
  auto& mesh = nameMeshMap[name];
  glDeleteBuffers(1, &mesh.vertexBuffer);
  glDeleteBuffers(1, &mesh.indexBuffer);
  glDeleteVertexArrays(1, &mesh.vertexArray);
  meshIndexNameMap.erase(mesh.vertexArray);
  nameMeshMap.erase(name);
  meshHashNameMap.erase(meshNameHashMap[name]);
  meshNameHashMap.erase(name);
}
Mesh AssetManager::CreateVertexBuffer(const std::vector<float>& vertices, bool withNormals) {
  Mesh mesh;
  auto stride = withNormals ? 6 : 3;
  mesh.vertexCount = vertices.size() / stride;
  glGenVertexArrays(1, &mesh.vertexArray);
  glBindVertexArray(mesh.vertexArray);
  glGenBuffers(1, &mesh.vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
  if (withNormals) {
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
  }
  return mesh;
}
void AssetManager::CreateIndexBuffer(Mesh& mesh, const std::vector<unsigned int>& indices) {
  mesh.indexCount = indices.size();
  glBindVertexArray(mesh.vertexArray);
  glGenBuffers(1, &mesh.indexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
}
std::size_t AssetManager::ComputeMeshHash(const std::vector<float>& vertices) const {
  auto seed = vertices.size();
  for (const auto& v : vertices)
    seed ^= std::hash<float>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  return seed;
}
