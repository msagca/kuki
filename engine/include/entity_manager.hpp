#pragma once
#include <component/camera.hpp>
#include <component/component.hpp>
#include <component_manager.hpp>
#include <event_dispatcher.hpp>
#include <kuki_export.h>
#include <set>
#include <string>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <utility/octree.hpp>
#include <utility/trie.hpp>
#include <vector>
namespace kuki {
/// @brief Manages entities and their components in a scene
class KUKI_API EntityManager {
private:
  unsigned int nextId{};
  std::set<unsigned int> ids;
  Trie names;
  Octree<unsigned int> octree;
  std::unordered_map<ComponentType, std::type_index> idToType; // NOTE: this is component type Id, not entity Id
  std::unordered_map<std::string, std::type_index> nameToType;
  std::unordered_map<std::string, unsigned int> nameToId;
  std::unordered_map<std::type_index, ComponentMask> typeToMask;
  std::unordered_map<std::type_index, IComponentManager*> typeToManager;
  std::unordered_map<std::type_index, IEventDispatcher*> typeToDispatcher;
  std::unordered_map<unsigned int, std::string> idToName;
  std::unordered_map<unsigned int, std::set<unsigned int>> idToChildren;
  std::unordered_map<unsigned int, unsigned int> idToParent;
  template <typename T>
  ComponentManager<T>* GetManager();
  IComponentManager* GetManager(std::type_index);
  IComponentManager* GetManager(const std::string&);
  IComponentManager* GetManager(ComponentType);
  template <typename T>
  EventDispatcher<T>* GetEventDispatcher();
  template <typename T>
  size_t GetComponentMask() const;
  void DeleteRecords(unsigned int);
  void UpdateOctree();
public:
  ~EntityManager();
  unsigned int Create(std::string&);
  void Delete(unsigned int);
  void Delete(const std::string&);
  void DeleteAll();
  void DeleteAll(const std::string&);
  bool Rename(unsigned int, std::string&);
  const std::string& GetName(unsigned int) const;
  int GetId(const std::string&);
  /// @brief Create parent-child relationship between the given entities
  /// @return true if the operation was successful, false otherwise
  bool AddChild(unsigned int, unsigned int);
  /// @brief Remove the parent-child relationship between the given entities
  void RemoveChild(unsigned int, unsigned int);
  bool HasChildren(unsigned int) const;
  bool HasParent(unsigned int) const;
  int GetParent(unsigned int) const;
  size_t GetCount() const;
  template <typename T>
  T* AddComponent(unsigned int);
  IComponent* AddComponent(unsigned int, ComponentType);
  IComponent* AddComponent(unsigned int, const std::string&);
  template <typename... T>
  std::tuple<T*...> AddComponents(unsigned int);
  template <typename T>
  void RemoveComponent(unsigned int);
  void RemoveComponent(unsigned int, ComponentType);
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
  /// @brief Get the first component of the specified type
  /// @return A pointer to the first component, or nullptr if no such component exists
  template <typename T>
  T* GetFirstComponent();
  std::vector<IComponent*> GetAllComponents(unsigned int);
  std::vector<std::string> GetMissingComponents(unsigned int);
  /// @brief Execute a function on entities with specified components
  template <typename... T, typename F>
  void ForEach(F);
  /// @brief Execute a function on all children of a given entity
  template <typename F>
  void ForEachChild(unsigned int, F);
  /// @brief Execute a function on all root entities
  template <typename F>
  void ForEachRoot(F);
  /// @brief Execute a function on all entities
  template <typename F>
  void ForAll(F);
  template <typename F>
  void ForEachVisibleEntity(const Camera&, F);
  template <typename F>
  void ForEachOctreeNode(F);
  template <typename F>
  void ForEachOctreeLeafNode(F);
  template <typename T, typename F>
  unsigned int RegisterCallback(F&&);
  template <typename T>
  void UnregisterCallback(unsigned int);
};
template <typename T>
T* EntityManager::AddComponent(unsigned int id) {
  auto manager = GetManager<T>();
  if (manager->Has(id))
    return manager->Get(id);
  T* component = &manager->Add(id);
  return component;
}
template <typename... T>
std::tuple<T*...> EntityManager::AddComponents(unsigned int id) {
  if (ids.find(id) == ids.end())
    return std::tuple<T*...>{};
  return std::tie(AddComponent<T>(id)...);
}
template <typename T>
void EntityManager::RemoveComponent(unsigned int id) {
  GetManager<T>()->Remove(id);
}
template <typename... T>
void EntityManager::RemoveComponents(unsigned int id) {
  if (ids.find(id) == ids.end())
    return;
  (RemoveComponent<T>(id), ...);
}
template <typename T>
bool EntityManager::HasComponent(unsigned int id) {
  return GetManager<T>()->Has(id);
}
template <typename... T>
bool EntityManager::HasComponents(unsigned int id) {
  if (ids.find(id) == ids.end())
    return false;
  return (HasComponent<T>(id) && ...);
}
template <typename T>
T* EntityManager::GetComponent(unsigned int id) {
  return GetManager<T>()->Get(id);
}
template <typename... T>
std::tuple<T*...> EntityManager::GetComponents(unsigned int id) {
  return std::make_tuple(GetComponent<T>(id)...);
}
template <typename T>
T* EntityManager::GetFirstComponent() {
  return GetManager<T>()->GetFirst();
}
template <typename... T, typename F>
void EntityManager::ForEach(F func) {
  for (const auto id : ids)
    if (HasComponents<T...>(id)) {
      auto components = GetComponents<T...>(id);
      std::apply([&](T*... args) { func(id, args...); }, components);
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
  for (const auto id : ids)
    if (!HasParent(id))
      func(id);
}
template <typename F>
void EntityManager::ForAll(F func) {
  for (const auto id : ids)
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
    idToType.emplace(ComponentTraits<T>::GetId(), type);
    typeToMask.emplace(type, ComponentTraits<T>::GetMask());
  }
  return static_cast<ComponentManager<T>*>(typeToManager[type]);
}
template <typename T>
size_t EntityManager::GetComponentMask() const {
  auto type = std::type_index(typeid(T));
  auto it = typeToMask.find(type);
  if (it == typeToMask.end())
    return 0;
  return static_cast<size_t>(it->second);
}
template <typename T>
EventDispatcher<T>* EntityManager::GetEventDispatcher() {
  auto type = std::type_index(typeid(T));
  auto it = typeToDispatcher.find(type);
  if (it == typeToDispatcher.end())
    typeToDispatcher.emplace(type, new EventDispatcher<T>());
  return static_cast<EventDispatcher<T>*>(typeToDispatcher[type]);
}
template <typename F>
void EntityManager::ForEachVisibleEntity(const Camera& camera, F func) {
  UpdateOctree();
  octree.ForEachInFrustum(camera, [&](unsigned int id) {
    func(id);
  });
}
template <typename F>
void EntityManager::ForEachOctreeNode(F func) {
  octree.ForEach(func);
}
template <typename F>
void EntityManager::ForEachOctreeLeafNode(F func) {
  octree.ForEachLeaf(func);
}
template <typename T, typename F>
unsigned int EntityManager::RegisterCallback(F&& callback) {
  auto dispatcher = GetEventDispatcher<T>();
  return dispatcher->Subscribe(std::forward<F>(callback));
}
template <typename T>
void EntityManager::UnregisterCallback(unsigned int id) {
  auto dispatcher = GetEventDispatcher<T>();
  dispatcher->Unsubscribe(id);
}
} // namespace kuki
