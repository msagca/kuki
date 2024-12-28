#pragma once
#include <component_manager.hpp>
#include <component_types.hpp>
#include <cstdint>
#include <primitive.hpp>
#include <string>
#include <unordered_map>
#include <vector>
/// <summary>
/// This is very much like the EntityManager class, but for managing things that are not necessarily in a scene
/// </summary>
class AssetManager {
private:
  unsigned int nextID = 0;
  std::unordered_map<unsigned int, uint32_t> idToMask;
  std::unordered_map<unsigned int, std::string> idToName;
  std::unordered_map<std::string, unsigned int> nameToID;
  ComponentManager<Transform> transformManager;
  ComponentManager<Mesh> meshManager;
  ComponentManager<Texture> textureManager;
  std::string GenerateName(const std::string&);
  template <typename T>
  ComponentManager<T>& GetManager();
  template <typename T>
  ComponentMask GetComponentMask() const;
  Mesh CreateVertexBuffer(const std::vector<Vertex>&);
  void CreateIndexBuffer(Mesh&, const std::vector<unsigned int>&);
public:
  unsigned int Create(std::string = "");
  void Remove(unsigned int);
  bool Rename(unsigned int, std::string);
  const std::string& GetName(unsigned int);
  template <typename T>
  T& AddComponent(unsigned int);
  template <typename T>
  void RemoveComponent(unsigned int);
  void RemoveAllComponents(unsigned int);
  template <typename T>
  bool HasComponent(unsigned int);
  template <typename T>
  T& GetComponent(unsigned int);
  Mesh CreateMesh(const std::vector<Vertex>&);
  Mesh CreateMesh(const std::vector<Vertex>&, const std::vector<unsigned int>&);
  BoundingBox CalculateBoundingBox(const std::vector<Vertex>&);
  void CleanUp();
};
template <typename T>
T& AssetManager::AddComponent(unsigned int id) {
  auto& manager = GetManager<T>();
  if (idToMask.find(id) == idToMask.end())
    return manager.GetDefault();
  if (HasComponent<T>(id))
    return manager.Get(id);
  idToMask[id] |= GetComponentMask<T>();
  return manager.Add(id);
}
template <typename T>
void AssetManager::RemoveComponent(unsigned int id) {
  if (idToMask.find(id) == idToMask.end())
    return;
  idToMask[id] &= ~GetComponentMask<T>();
  auto& manager = GetManager<T>();
  manager.Remove(id);
}
template <typename T>
bool AssetManager::HasComponent(unsigned int id) {
  if (idToMask.find(id) == idToMask.end())
    return false;
  return (idToMask[id] & GetComponentMask<T>()) != 0;
}
template <typename T>
T& AssetManager::GetComponent(unsigned int id) {
  auto& manager = GetManager<T>();
  return manager.GetComponent(id);
}
template <>
inline ComponentManager<Transform>& AssetManager::GetManager() {
  return transformManager;
}
template <>
inline ComponentManager<Mesh>& AssetManager::GetManager() {
  return meshManager;
}
template <>
inline ComponentManager<Texture>& AssetManager::GetManager() {
  return textureManager;
}
template <>
inline ComponentMask AssetManager::GetComponentMask<Transform>() const {
  return TransformMask;
}
template <>
inline ComponentMask AssetManager::GetComponentMask<Mesh>() const {
  return MeshMask;
}
template <>
inline ComponentMask AssetManager::GetComponentMask<Texture>() const {
  return TextureMask;
}
