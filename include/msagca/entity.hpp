#pragma once
#include <component.hpp>
#include <functional>
#include <tuple>
#include <format>
enum ComponentMask {
  TransformMask = 1 << 0,
  MeshFilterMask = 1 << 1,
  MeshRendererMask = 1 << 2
};
class EntityManager {
private:
  unsigned int nextID = 0;
  std::unordered_map<unsigned int, uint32_t> entities; // key: entity ID, value: component mask
  std::unordered_map<unsigned int, std::string> names; // key: entity ID, value: entity name
  ComponentManager<Transform> transformManager;
  ComponentManager<MeshFilter> filterManager;
  ComponentManager<MeshRenderer> rendererManager;
  template <typename T>
  ComponentMask GetComponentMask() const;
public:
  unsigned int CreateEntity(std::string = "");
  void RemoveEntity(unsigned int);
  void RenameEntity(unsigned int, std::string);
  const std::string& GetName(unsigned int);
  size_t GetCount() const;
  void CleanUp();
  template <typename T>
  T& AddComponent(unsigned int);
  template <typename... T>
  std::tuple<T&...> AddComponents(unsigned int);
  template <typename T>
  void RemoveComponent(unsigned int);
  template <typename... T>
  void RemoveComponents(unsigned int);
  void RemoveAllComponents(unsigned int);
  template <typename T>
  bool HasComponent(unsigned int);
  template <typename... T>
  bool HasComponents(unsigned int);
  template <typename T>
  T& GetComponent(unsigned int);
  template <typename... T>
  std::tuple<T&...> GetComponents(unsigned int);
  std::vector<std::unique_ptr<IComponent>> GetAllComponents(unsigned int);
  template <typename T>
  ComponentManager<T>& GetManager();
  template <typename... T>
  void ForEach(std::function<void(unsigned int)>);
  template <typename... T>
  void ForEach(std::function<void(unsigned int, T&...)>);
};
inline unsigned int EntityManager::CreateEntity(std::string name) {
  entities[nextID] = 0;
  if (name.size() > 0)
    // TODO: check if the name is available
    names[nextID] = name;
  else
    names[nextID] = std::format("Entity {}", nextID);
  return nextID++;
}
inline void EntityManager::RemoveEntity(unsigned int id) {
  RemoveAllComponents(id);
  entities.erase(id);
  names.erase(id);
}
inline void EntityManager::RenameEntity(unsigned int id, std::string name) {
  if (names.find(id) == names.end() || name.size() == 0)
    return;
  // TODO: check if the name is available
  names[id] = name;
}
inline const std::string& EntityManager::GetName(unsigned int id) {
  static const std::string emptyString = "";
  if (names.find(id) == names.end())
    return emptyString;
  return names[id];
}
inline size_t EntityManager::GetCount() const {
  return entities.size();
}
inline void EntityManager::CleanUp() {
  filterManager.CleanUp();
  rendererManager.CleanUp();
}
template <typename T>
T& EntityManager::AddComponent(unsigned int id) {
  auto& manager = GetManager<T>();
  if (names.find(id) == names.end())
    return manager.GetDefault();
  entities[id] |= GetComponentMask<T>();
  return manager.AddComponent(id);
}
template <typename... T>
std::tuple<T&...> EntityManager::AddComponents(unsigned int id) {
  return std::tie(AddComponent<T>(id)...);
}
template <typename T>
void EntityManager::RemoveComponent(unsigned int id) {
  if (entities.find(id) == entities.end())
    return;
  entities[id] &= ~GetComponentMask<T>();
  auto& manager = GetManager<T>();
  manager.RemoveComponent(id);
}
template <typename... T>
void EntityManager::RemoveComponents(unsigned int id) {
  if (entities.find(id) == entities.end())
    return;
  (RemoveComponent<T>(id), ...);
}
inline void EntityManager::RemoveAllComponents(unsigned int id) {
  if (entities.find(id) == entities.end())
    return;
  // TODO: make this a loop
  transformManager.RemoveComponent(id);
  filterManager.RemoveComponent(id);
  rendererManager.RemoveComponent(id);
  entities[id] = 0;
}
template <typename T>
bool EntityManager::HasComponent(unsigned int id) {
  if (entities.find(id) == entities.end())
    return false;
  return (entities[id] & GetComponentMask<T>()) != 0;
}
template <typename... T>
bool EntityManager::HasComponents(unsigned int id) {
  if (entities.find(id) == entities.end())
    return false;
  auto combinedMask = (GetComponentMask<T>() | ...);
  return (entities[id] & combinedMask) == combinedMask;
}
template <typename T>
T& EntityManager::GetComponent(unsigned int id) {
  auto& manager = GetManager<T>();
  return manager.GetComponent(id);
}
template <typename... T>
std::tuple<T&...> EntityManager::GetComponents(unsigned int id) {
  return std::tie(GetComponent<T>(id)...);
}
inline std::vector<std::unique_ptr<IComponent>> EntityManager::GetAllComponents(unsigned int id) {
  std::vector<std::unique_ptr<IComponent>> components;
  // TODO: make this a loop
  if (HasComponent<Transform>(id))
    components.push_back(transformManager.GetComponentPtr(id));
  if (HasComponent<MeshFilter>(id))
    components.push_back(filterManager.GetComponentPtr(id));
  if (HasComponent<MeshRenderer>(id))
    components.push_back(rendererManager.GetComponentPtr(id));
  return components;
}
template <typename... T>
void EntityManager::ForEach(std::function<void(unsigned int)> func) {
  for (const auto& [id, _] : entities)
    if (HasComponents<T...>(id))
      func(id);
}
template <typename... T>
void EntityManager::ForEach(std::function<void(unsigned int, T&...)> func) {
  for (const auto& [id, _] : entities)
    if (HasComponents<T...>(id)) {
      auto components = GetComponents<T...>(id);
      std::apply([&](T&... args) { func(id, args...); }, components);
    }
}
template <>
inline ComponentManager<Transform>& EntityManager::GetManager() {
  return transformManager;
}
template <>
inline ComponentManager<MeshFilter>& EntityManager::GetManager() {
  return filterManager;
}
template <>
inline ComponentManager<MeshRenderer>& EntityManager::GetManager() {
  return rendererManager;
}
template <>
inline ComponentMask EntityManager::GetComponentMask<Transform>() const {
  return TransformMask;
}
template <>
inline ComponentMask EntityManager::GetComponentMask<MeshFilter>() const {
  return MeshFilterMask;
}
template <>
inline ComponentMask EntityManager::GetComponentMask<MeshRenderer>() const {
  return MeshRendererMask;
}
