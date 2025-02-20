#pragma once
#include <component/camera.hpp>
#include <component/component.hpp>
#include <component/light.hpp>
#include <component/mesh_filter.hpp>
#include <component/mesh_renderer.hpp>
#include <component/transform.hpp>
#include <component_manager.hpp>
#include <engine_export.h>
#include <string>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
/// <summary>
/// Manages entities and their components in a scene
/// </summary>
class ENGINE_API EntityManager {
private:
  unsigned int nextID = 0;
  std::unordered_map<unsigned int, size_t> idToMask;
  std::unordered_map<unsigned int, std::string> idToName;
  std::unordered_map<std::string, unsigned int> nameToID;
  std::unordered_map<unsigned int, std::unordered_set<unsigned int>> idToChildren;
  std::unordered_map<unsigned int, unsigned int> idToParent;
  std::unordered_map<std::type_index, IComponentManager*> managers;
  template <typename T>
  ComponentManager<T>* GetManager();
  template <typename T>
  size_t GetComponentMask() const;
  std::string GenerateName(const std::string&);
public:
  ~EntityManager();
  unsigned int Create(std::string = "");
  void Delete(unsigned int);
  bool Rename(unsigned int, std::string);
  const std::string& GetName(unsigned int) const;
  int GetID(const std::string&);
  void AddChild(unsigned int, unsigned int);
  void RemoveChild(unsigned int, unsigned int);
  bool HasChildren(unsigned int) const;
  bool HasParent(unsigned int) const;
  int GetParent(unsigned int) const;
  size_t GetCount() const;
  template <typename T>
  T* AddComponent(unsigned int);
  IComponent* AddComponent(unsigned int, ComponentID);
  IComponent* AddComponent(unsigned int, const std::string&);
  template <typename... T>
  std::tuple<T*...> AddComponents(unsigned int);
  template <typename T>
  void RemoveComponent(unsigned int);
  void RemoveComponent(unsigned int, ComponentID);
  void RemoveComponent(unsigned int, const std::string&);
  template <typename... T>
  void RemoveComponents(unsigned int);
  void RemoveAllComponents(unsigned int);
  template <typename T>
  bool HasComponent(unsigned int);
  template <typename... T>
  bool HasComponents(unsigned int);
  template <typename T>
  T* GetComponent(unsigned int);
  IComponent* GetComponent(unsigned int, const std::string&);
  template <typename... T>
  std::tuple<T*...> GetComponents(unsigned int);
  template <typename T>
  T* GetFirstComponent();
  std::vector<IComponent*> GetAllComponents(unsigned int);
  std::vector<std::string> GetMissingComponents(unsigned int);
  template <typename... T, typename F>
  void ForEach(F);
  template <typename F>
  void ForEachChild(unsigned int, F);
  template <typename F>
  void ForAll(F);
};
template <typename T>
T* EntityManager::AddComponent(unsigned int id) {
  if (idToMask.find(id) == idToMask.end())
    return nullptr;
  auto manager = GetManager<T>();
  idToMask[id] |= GetComponentMask<T>();
  return &manager->Add(id);
}
template <typename... T>
std::tuple<T*...> EntityManager::AddComponents(unsigned int id) {
  return std::tie(AddComponent<T>(id)...);
}
template <typename T>
void EntityManager::RemoveComponent(unsigned int id) {
  if (idToMask.find(id) == idToMask.end())
    return;
  idToMask[id] &= ~GetComponentMask<T>();
  auto manager = GetManager<T>();
  manager->Remove(id);
}
template <typename... T>
void EntityManager::RemoveComponents(unsigned int id) {
  (RemoveComponent<T>(id), ...);
}
template <typename T>
bool EntityManager::HasComponent(unsigned int id) {
  if (idToMask.find(id) == idToMask.end())
    return false;
  return (idToMask[id] & GetComponentMask<T>()) != 0;
}
template <typename... T>
bool EntityManager::HasComponents(unsigned int id) {
  auto it = idToMask.find(id);
  if (it == idToMask.end())
    return false;
  auto combinedMask = (GetComponentMask<T>() | ...);
  return (it->second & combinedMask) == combinedMask;
}
template <typename T>
T* EntityManager::GetComponent(unsigned int id) {
  auto manager = GetManager<T>();
  return manager->Get(id);
}
template <typename... T>
std::tuple<T*...> EntityManager::GetComponents(unsigned int id) {
  return std::make_tuple(GetComponent<T>(id)...);
}
template <typename T>
T* EntityManager::GetFirstComponent() {
  auto manager = GetManager<T>();
  return manager->GetFirst();
}
template <typename... T, typename F>
void EntityManager::ForEach(F func) {
  for (const auto& [id, _] : idToMask)
    if (HasComponents<T...>(id)) {
      auto components = GetComponents<T...>(id);
      std::apply([&](T*... args) { func(args...); }, components);
    }
}
template <typename F>
void EntityManager::ForEachChild(unsigned int parent, F func) {
  auto it = idToChildren.find(parent);
  if (it == idToChildren.end())
    return;
  for (auto child : it->second)
    func(child, idToName[child]);
}
template <typename F>
void EntityManager::ForAll(F func) {
  for (const auto& [id, _] : idToMask)
    func(id);
}
template <typename T>
ComponentManager<T>* EntityManager::GetManager() {
  auto it = managers.find(std::type_index(typeid(T)));
  if (it == managers.end())
    managers[std::type_index(typeid(T))] = new ComponentManager<T>();
  return static_cast<ComponentManager<T>*>(managers[std::type_index(typeid(T))]);
}
template <>
inline size_t EntityManager::GetComponentMask<Camera>() const {
  return static_cast<size_t>(ComponentMask::Camera);
}
template <>
inline size_t EntityManager::GetComponentMask<Light>() const {
  return static_cast<size_t>(ComponentMask::Light);
}
template <>
inline size_t EntityManager::GetComponentMask<Material>() const {
  return static_cast<size_t>(ComponentMask::Material);
}
template <>
inline size_t EntityManager::GetComponentMask<MeshFilter>() const {
  return static_cast<size_t>(ComponentMask::MeshFilter);
}
template <>
inline size_t EntityManager::GetComponentMask<Mesh>() const {
  return static_cast<size_t>(ComponentMask::Mesh);
}
template <>
inline size_t EntityManager::GetComponentMask<MeshRenderer>() const {
  return static_cast<size_t>(ComponentMask::MeshRenderer);
}
template <>
inline size_t EntityManager::GetComponentMask<Texture>() const {
  return static_cast<size_t>(ComponentMask::Texture);
}
template <>
inline size_t EntityManager::GetComponentMask<Transform>() const {
  return static_cast<size_t>(ComponentMask::Transform);
}
