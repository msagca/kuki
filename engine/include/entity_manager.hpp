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
#include <utility/trie.hpp>
#include <vector>
/// <summary>
/// Manages entities and their components in a scene
/// </summary>
class ENGINE_API EntityManager {
private:
  unsigned int nextID = 0;
  std::unordered_set<unsigned int> ids;
  Trie names;
  std::unordered_map<ComponentID, std::type_index> idToType; // NOTE: this is component type ID, not entity ID
  std::unordered_map<std::string, std::type_index> nameToType;
  std::unordered_map<std::string, unsigned int> nameToID;
  std::unordered_map<std::type_index, ComponentMask> typeToMask;
  std::unordered_map<std::type_index, IComponentManager*> typeToManager;
  std::unordered_map<unsigned int, std::string> idToName;
  std::unordered_map<unsigned int, std::unordered_set<unsigned int>> idToChildren;
  std::unordered_map<unsigned int, unsigned int> idToParent;
  template <typename T>
  ComponentManager<T>* GetManager();
  IComponentManager* GetManager(std::type_index);
  IComponentManager* GetManager(const std::string&);
  IComponentManager* GetManager(ComponentID);
  template <typename T>
  size_t GetComponentMask() const;
  void DeleteRecords(unsigned int);
public:
  ~EntityManager();
  unsigned int Create(std::string&);
  void Delete(unsigned int);
  bool Rename(unsigned int, std::string&);
  std::string GetName(unsigned int) const;
  int GetID(const std::string&);
  /// <summary>
  /// Create parent-child relationship between the given entities
  /// </summary>
  /// <returns>true if the operation was successful, false otherwise</returns>
  bool AddChild(unsigned int, unsigned int);
  /// <summary>
  /// Remove the parent-child relationship between the given entities
  /// </summary>
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
  bool HasComponent(unsigned int, std::type_index);
  template <typename... T>
  bool HasComponents(unsigned int);
  template <typename T>
  T* GetComponent(unsigned int);
  IComponent* GetComponent(unsigned int, const std::string&);
  template <typename... T>
  std::tuple<T*...> GetComponents(unsigned int);
  /// <summary>
  /// Get the first component of the specified type
  /// </summary>
  /// <returns>A pointer to the first component or nullptr if no such component exists</returns>
  template <typename T>
  T* GetFirstComponent();
  std::vector<IComponent*> GetAllComponents(unsigned int);
  std::vector<std::string> GetMissingComponents(unsigned int);
  /// <summary>
  /// Execute a function on entities with specified components
  /// </summary>
  template <typename... T, typename F>
  void ForEach(F);
  /// <summary>
  /// Execute a function on all children of a given entity
  /// </summary>
  template <typename F>
  void ForEachChild(unsigned int, F);
  /// <summary>
  /// Execute a function on all root entities
  /// </summary>
  template <typename F>
  void ForEachRoot(F);
  /// <summary>
  /// Execute a function on all entities
  /// </summary>
  template <typename F>
  void ForAll(F);
};
template <typename T>
T* EntityManager::AddComponent(unsigned int id) {
  auto manager = GetManager<T>();
  if (!manager)
    return nullptr;
  if (manager->Has(id))
    return manager->Get(id);
  return &manager->Add(id);
}
template <typename... T>
std::tuple<T*...> EntityManager::AddComponents(unsigned int id) {
  if (ids.find(id) == ids.end())
    return std::tuple<T*...>{};
  return std::tie(AddComponent<T>(id)...);
}
template <typename T>
void EntityManager::RemoveComponent(unsigned int id) {
  auto manager = GetManager<T>();
  if (!manager)
    return;
  manager->Remove(id);
}
template <typename... T>
void EntityManager::RemoveComponents(unsigned int id) {
  if (ids.find(id) == ids.end())
    return;
  (RemoveComponent<T>(id), ...);
}
template <typename T>
bool EntityManager::HasComponent(unsigned int id) {
  auto manager = GetManager<T>();
  if (!manager)
    return false;
  return manager->Has(id);
}
template <typename... T>
bool EntityManager::HasComponents(unsigned int id) {
  if (ids.find(id) == ids.end())
    return false;
  return (HasComponent<T>(id) && ...);
}
template <typename T>
T* EntityManager::GetComponent(unsigned int id) {
  auto manager = GetManager<T>();
  if (!manager)
    return nullptr;
  return manager->Get(id);
}
template <typename... T>
std::tuple<T*...> EntityManager::GetComponents(unsigned int id) {
  return std::make_tuple(GetComponent<T>(id)...);
}
template <typename T>
T* EntityManager::GetFirstComponent() {
  auto manager = GetManager<T>();
  if (!manager)
    return nullptr;
  return manager->GetFirst();
}
template <typename... T, typename F>
void EntityManager::ForEach(F func) {
  for (const auto& id : ids)
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
    func(child);
}
template <typename F>
void EntityManager::ForEachRoot(F func) {
  for (const auto& id : ids)
    if (!HasParent(id))
      func(id);
}
template <typename F>
void EntityManager::ForAll(F func) {
  for (const auto& id : ids)
    func(id);
}
template <typename T>
ComponentManager<T>* EntityManager::GetManager() {
  static_assert(std::is_base_of<IComponent, T>::value, "T must inherit from IComponent.");
  auto type = std::type_index(typeid(T));
  auto it = typeToManager.find(type);
  if (it == typeToManager.end()) {
    typeToManager.emplace(type, new ComponentManager<T>());
    nameToType.emplace(ComponentTraits<T>::GetName(), type);
    idToType.emplace(ComponentTraits<T>::GetID(), type);
    typeToMask.emplace(type, ComponentTraits<T>::GetMask());
  }
  return static_cast<ComponentManager<T>*>(typeToManager[type]);
}
template <typename T>
size_t EntityManager::GetComponentMask() const {
  auto it = typeToMask.find(std::type_index(typeid(T)));
  if (it == typeToMask.end())
    return 0;
  return static_cast<size_t>(it->second);
}
