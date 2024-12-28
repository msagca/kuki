#include <glm/common.hpp>
#include <asset_manager.hpp>
#include <chrono>
#include <component_types.hpp>
#include <cstddef>
#include <glad/glad.h>
#include <glm/ext/vector_float3.hpp>
#include <limits>
#include <primitive.hpp>
#include <string>
#include <vector>
unsigned int AssetManager::Create(std::string name) {
  if (name.size() > 0) {
    auto assetName = name;
    if (nameToID.find(name) != nameToID.end())
      assetName = GenerateName(name);
    idToName[nextID] = assetName;
  } else
    idToName[nextID] = GenerateName("Asset#");
  nameToID[idToName[nextID]] = nextID;
  idToMask[nextID] = 0;
  return nextID++;
}
std::string AssetManager::GenerateName(const std::string& name) {
  auto now = std::chrono::high_resolution_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
  std::hash<std::string> hashFunc;
  auto hash = hashFunc(name + std::to_string(timestamp));
  return name + std::to_string(hash);
}
void AssetManager::Remove(unsigned int id) {
  RemoveAllComponents(id);
  idToMask.erase(id);
  nameToID.erase(idToName[id]);
  idToName.erase(id);
}
bool AssetManager::Rename(unsigned int id, std::string name) {
  if (idToName.find(id) == idToName.end() || name.size() == 0 || nameToID.find(name) != nameToID.end())
    return false;
  nameToID.erase(idToName[id]);
  nameToID[name] = id;
  idToName[id] = name;
  return true;
}
const std::string& AssetManager::GetName(unsigned int id) {
  static const std::string emptyString = "";
  auto it = idToName.find(id);
  if (it != idToName.end())
    return it->second;
  return emptyString;
}
void AssetManager::RemoveAllComponents(unsigned int id) {
  if (idToMask.find(id) == idToMask.end())
    return;
  transformManager.Remove(id);
  meshManager.Remove(id);
  textureManager.Remove(id);
  idToMask[id] = 0;
}
Mesh AssetManager::CreateMesh(const std::vector<Vertex>& vertices) {
  auto mesh = CreateVertexBuffer(vertices);
  return mesh;
}
Mesh AssetManager::CreateMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
  auto mesh = CreateMesh(vertices);
  CreateIndexBuffer(mesh, indices);
  return mesh;
}
BoundingBox AssetManager::CalculateBoundingBox(const std::vector<Vertex>& vertices) {
  BoundingBox box;
  box.min = glm::vec3(std::numeric_limits<float>::max());
  box.max = glm::vec3(std::numeric_limits<float>::lowest());
  for (const auto& v : vertices) {
    box.min = glm::min(box.min, v.position);
    box.max = glm::max(box.max, v.position);
  }
  return box;
}
Mesh AssetManager::CreateVertexBuffer(const std::vector<Vertex>& vertices) {
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
  return mesh;
}
void AssetManager::CreateIndexBuffer(Mesh& mesh, const std::vector<unsigned int>& indices) {
  mesh.indexCount = indices.size();
  glBindVertexArray(mesh.vertexArray);
  glGenBuffers(1, &mesh.indexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
}
void AssetManager::CleanUp() {
  meshManager.CleanUp();
  textureManager.CleanUp();
}
