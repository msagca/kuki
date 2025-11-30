#pragma once
#include <component.hpp>
#include <component_manager.hpp>
#include <component_traits.hpp>
#include <id.hpp>
#include <trie.hpp>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace kuki {
template <typename T>
concept IsComponent = std::is_base_of_v<IComponent, T>;
/// @brief Manages entities and their components in a scene
class KUKI_ENGINE_API EntityManager {
private:
  Trie<SuffixNode> names; // TODO: no need to have unique names, entities already have unique IDs
  std::unordered_map<ComponentType, std::type_index> idToType;
  std::unordered_map<ID, ID> idToParent;
  std::unordered_map<ID, std::string> idToName;
  std::unordered_map<ID, std::unordered_set<ID>> idToChildren;
  std::unordered_map<std::string, ID> nameToId;
  std::unordered_map<std::string, std::type_index> nameToType;
  std::unordered_map<std::type_index, ComponentMask> typeToMask;
  std::unordered_map<std::type_index, IComponentManager*> typeToManager;
  // TODO: implement spatial partitioning, keep a ComponentManager per quadrant/octant for certain component types (e.g., Transform)
  std::unordered_set<ID> ids;
  template <IsComponent C>
  ComponentManager<C>* GetManager();
  IComponentManager* GetManager(std::type_index);
  IComponentManager* GetManager(const std::string&);
  IComponentManager* GetManager(ComponentType);
  template <typename C>
  size_t GetComponentMask() const;
  void DeleteRecords(const ID);
public:
  ~EntityManager();
  ID Create(std::string&);
  void Delete(const ID);
  void Delete(const std::string&);
  void DeleteAll();
  void DeleteAll(const std::string&);
  bool Rename(const ID, std::string&);
  bool IsEntity(const ID);
  const std::string& GetName(const ID) const;
  ID GetId(const std::string&);
  /// @brief Create parent-child relationship between the given entities
  /// @param parent Parent entity ID
  /// @param child Child entity ID
  /// @param keepWorld Preserve child's world transform
  /// @return true if the operation was successful, false otherwise
  bool AddChild(const ID, const ID, bool = false);
  /// @brief Remove the parent-child relationship between the given entities
  void RemoveChild(const ID, const ID);
  bool HasChildren(const ID) const;
  bool HasParent(const ID) const;
  ID GetParent(const ID) const;
  size_t GetCount() const;
  template <typename C>
  C* AddComponent(const ID);
  IComponent* AddComponent(const ID, ComponentType);
  IComponent* AddComponent(const ID, const std::string&);
  template <typename... C>
  std::tuple<C*...> AddComponents(ID);
  template <typename C>
  void RemoveComponent(const ID);
  void RemoveComponent(const ID, ComponentType);
  void RemoveComponent(const ID, const std::string&);
  template <typename... C>
  void RemoveComponents(const ID);
  void RemoveAllComponents(const ID);
  template <typename C>
  bool HasComponent(const ID);
  bool HasComponent(const ID, std::type_index);
  template <typename... C>
  bool HasComponents(const ID);
  template <typename C>
  C* GetComponent(const ID);
  IComponent* GetComponent(const ID, ComponentType);
  IComponent* GetComponent(const ID, const std::string&);
  template <typename... C>
  std::tuple<C*...> GetComponents(const ID);
  /// @brief Get the first component of the specified type
  /// @return A pointer to the first component, or nullptr if no such component exists
  template <typename C>
  C* GetFirstComponent();
  std::vector<IComponent*> GetAllComponents(const ID);
  std::vector<std::string> GetMissingComponents(const ID);
  template <typename C>
  void SortComponents();
  template <typename C>
  void UpdateComponents();
  void Update();
  /// @brief Execute a function on the first entity with specified components
  template <typename... C, typename F>
  void ForFirst(F&&);
  /// @brief Execute a function on entities with specified components
  template <typename... C, typename F>
  void ForEach(F&&);
  /// @brief Execute a function on all children of a given entity
  template <typename F>
  void ForEachChild(const ID, F&&);
  /// @brief Execute a function on all root entities
  template <typename F>
  void ForEachRoot(F&&);
  /// @brief Execute a function on all entities
  template <typename F>
  void ForAll(F&&);
};
template <typename T>
struct IsFalseType : std::false_type {};
template <typename C>
C* EntityManager::AddComponent(const ID id) {
  auto manager = GetManager<C>();
  if (manager->Has(id))
    return manager->Get(id);
  return &manager->Add(id);
}
template <typename... C>
std::tuple<C*...> EntityManager::AddComponents(const ID id) {
  return std::tie(AddComponent<C>(id)...);
}
template <typename C>
void EntityManager::RemoveComponent(const ID id) {
  GetManager<C>()->Remove(id);
}
template <typename... C>
void EntityManager::RemoveComponents(const ID id) {
  (RemoveComponent<C>(id), ...);
}
template <typename C>
bool EntityManager::HasComponent(const ID id) {
  return GetManager<C>()->Has(id);
}
template <typename... C>
bool EntityManager::HasComponents(const ID id) {
  return (HasComponent<C>(id) && ...);
}
template <typename C>
C* EntityManager::GetComponent(const ID id) {
  return GetManager<C>()->Get(id);
}
template <typename... C>
std::tuple<C*...> EntityManager::GetComponents(const ID id) {
  return std::make_tuple(GetComponent<C>(id)...);
}
template <typename C>
C* EntityManager::GetFirstComponent() {
  return GetManager<C>()->GetFirst();
}
template <typename... C, typename F>
void EntityManager::ForFirst(F&& func) {
  auto func_ = std::forward<F>(func);
  for (const auto& id : ids)
    if (HasComponents<C...>(id)) {
      auto components = GetComponents<C...>(id);
      std::apply([&](C*... args) { func_(id, args...); }, components);
      return;
    }
}
template <typename... C, typename F>
void EntityManager::ForEach(F&& func) {
  auto func_ = std::forward<F>(func);
  if constexpr (sizeof...(C) == 1) {
    using FirstC = std::tuple_element_t<0, std::tuple<C...>>;
    auto manager = GetManager<FirstC>();
    manager->ForEach([&](const ID id, FirstC* c) { func_(id, c); });
  } else
    for (const auto& id : ids)
      if (HasComponents<C...>(id)) {
        // TODO: implement archetypes for faster look up of certain component combinations
        auto components = GetComponents<C...>(id);
        std::apply([&](C*... args) { func_(id, args...); }, components);
      }
}
template <typename F>
void EntityManager::ForEachChild(const ID parent, F&& func) {
  auto func_ = std::forward<F>(func);
  if (auto it = idToChildren.find(parent); it != idToChildren.end())
    for (auto& child : it->second)
      func_(child);
}
template <typename F>
void EntityManager::ForEachRoot(F&& func) {
  auto func_ = std::forward<F>(func);
  for (const auto& id : ids)
    if (!HasParent(id))
      func_(id);
}
template <typename F>
void EntityManager::ForAll(F&& func) {
  auto func_ = std::forward<F>(func);
  for (const auto& id : ids)
    func_(id);
}
template <IsComponent C>
ComponentManager<C>* EntityManager::GetManager() {
  auto type = std::type_index(typeid(C));
  auto it = typeToManager.find(type);
  if (it == typeToManager.end()) {
    typeToManager.emplace(type, new ComponentManager<C>());
    nameToType.emplace(ComponentTraits<C>::GetName(), type);
    idToType.emplace(ComponentTraits<C>::GetType(), type);
    typeToMask.emplace(type, ComponentTraits<C>::GetMask());
  }
  return static_cast<ComponentManager<C>*>(typeToManager[type]);
}
template <typename C>
size_t EntityManager::GetComponentMask() const {
  auto type = std::type_index(typeid(C));
  auto it = typeToMask.find(type);
  if (it == typeToMask.end())
    return 0;
  return static_cast<size_t>(it->second);
}
template <typename C>
void EntityManager::SortComponents() {
  auto manager = GetManager<C>();
  if (!manager)
    return;
  manager->Sort();
}
template <typename C>
void EntityManager::UpdateComponents() {
  auto manager = GetManager<C>();
  if (!manager)
    return;
  manager->Update();
}
} // namespace kuki
