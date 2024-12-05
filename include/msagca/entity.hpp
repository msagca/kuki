#pragma once
#include <component.hpp>
#include <functional>
class EntityManager {
private:
  unsigned int nextID = 0;
  // TODO: keep track of entity-component pairs
  ComponentManager<Transform> transformManager;
  ComponentManager<MeshFilter> filterManager;
  ComponentManager<MeshRenderer> rendererManager;
public:
  unsigned int CreateEntity();
  unsigned int GetCount() const;
  template <typename T>
  T& AddComponent(unsigned int);
  template <typename T>
  void RemoveComponent(unsigned int);
  template <typename T>
  T& GetComponent(unsigned int);
  template <typename... T>
  void ForEach(std::function<void(unsigned int)>);
  template <typename T>
  ComponentManager<T>& GetManager();
  void CleanUp();
};
inline unsigned int EntityManager::CreateEntity() {
  return nextID++;
}
inline unsigned int EntityManager::GetCount() const {
  return nextID;
}
template <typename T>
T& EntityManager::AddComponent(unsigned int entityID) {
  auto& manager = GetManager<T>();
  return manager.AddComponent(entityID);
}
template <typename T>
void EntityManager::RemoveComponent(unsigned int entityID) {
  auto& manager = GetManager<T>();
  manager.RemoveComponent(entityID);
}
template <typename T>
T& EntityManager::GetComponent(unsigned int entityID) {
  auto& manager = GetManager<T>();
  return manager.GetComponent(entityID);
}
template <typename... T>
void EntityManager::ForEach(std::function<void(unsigned int)> func) {
  for (auto id = 0; id < GetCount(); id++) {
    auto hasAllComponents = (GetManager<T>().HasComponent(id) && ...);
    if (hasAllComponents)
      func(id);
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
inline void EntityManager::CleanUp() {
  filterManager.CleanUp();
  rendererManager.CleanUp();
}
