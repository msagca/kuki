#pragma once
#include "component/component.hpp"
#include "component/material.hpp"
#include "component/mesh.hpp"
#include "component/shader.hpp"
#include "component/texture.hpp"
#include "component/transform.hpp"
#include <component_manager.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
/// <summary>
/// This is very much like the EntityManager class, but for managing things that are not necessarily in a scene.
/// </summary>
class AssetManager {
private:
  unsigned int nextID = 0;
  std::unordered_map<unsigned int, size_t> idToMask;
  std::unordered_map<unsigned int, std::string> idToName;
  std::unordered_map<std::string, unsigned int> nameToID;
  std::unordered_map<unsigned int, std::unordered_set<unsigned int>> idToChildren;
  // TODO: assets shall only become components when they are loaded into a scene, relevant OpenGL buffers shall only be created then
  // TODO: keep a separate set for each asset type to store paths to assets of that type; define load functions for each asset
  ComponentManager<Material> materialManager;
  ComponentManager<Mesh> meshManager;
  ComponentManager<Shader> shaderManager;
  ComponentManager<Texture> textureManager;
  ComponentManager<Transform> transformManager;
  std::string GenerateName(const std::string&);
  template <typename T>
  ComponentManager<T>& GetManager();
  template <typename T>
  size_t GetComponentMask() const;
public:
  unsigned int Create(std::string = "");
  void Remove(unsigned int);
  bool Rename(unsigned int, std::string);
  const std::string& GetName(unsigned int);
  int GetID(const std::string&);
  void AddChild(unsigned int, unsigned int);
  void RemoveChild(unsigned int, unsigned int);
  template <typename F>
  void ForEachChild(unsigned int, F);
  template <typename T>
  T* AddComponent(unsigned int);
  template <typename T>
  void RemoveComponent(unsigned int);
  void RemoveAllComponents(unsigned int);
  template <typename T>
  bool HasComponent(unsigned int);
  template <typename T>
  T* GetComponent(unsigned int);
  std::vector<IComponent*> GetAllComponents(unsigned int);
  void CleanUp();
};
template <typename T>
T* AssetManager::AddComponent(unsigned int id) {
  if (idToMask.find(id) == idToMask.end())
    return nullptr;
  auto& manager = GetManager<T>();
  if (HasComponent<T>(id))
    return manager.Get(id);
  idToMask[id] |= GetComponentMask<T>();
  return &manager.Add(id);
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
T* AssetManager::GetComponent(unsigned int id) {
  auto& manager = GetManager<T>();
  return manager.Get(id);
}
template <typename F>
void AssetManager::ForEachChild(unsigned int parent, F func) {
  auto it = idToChildren.find(parent);
  if (it == idToChildren.end())
    return;
  for (auto child : it->second)
    func(child, idToName[child]);
}
template <>
inline ComponentManager<Material>& AssetManager::GetManager() {
  return materialManager;
}
template <>
inline ComponentManager<Mesh>& AssetManager::GetManager() {
  return meshManager;
}
template <>
inline ComponentManager<Shader>& AssetManager::GetManager() {
  return shaderManager;
}
template <>
inline ComponentManager<Texture>& AssetManager::GetManager() {
  return textureManager;
}
template <>
inline ComponentManager<Transform>& AssetManager::GetManager() {
  return transformManager;
}
template <>
inline size_t AssetManager::GetComponentMask<Material>() const {
  return static_cast<size_t>(ComponentMask::MaterialMask);
}
template <>
inline size_t AssetManager::GetComponentMask<Mesh>() const {
  return static_cast<size_t>(ComponentMask::MeshMask);
}
template <>
inline size_t AssetManager::GetComponentMask<Shader>() const {
  return static_cast<size_t>(ComponentMask::ShaderMask);
}
template <>
inline size_t AssetManager::GetComponentMask<Texture>() const {
  return static_cast<size_t>(ComponentMask::TextureMask);
}
template <>
inline size_t AssetManager::GetComponentMask<Transform>() const {
  return static_cast<size_t>(ComponentMask::TransformMask);
}
