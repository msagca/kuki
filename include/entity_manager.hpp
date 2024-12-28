#pragma once
#include <component_manager.hpp>
#include <component_types.hpp>
#include <tuple>
/// <summary>
/// Manages entities and their components in a scene
/// </summary>
class EntityManager {
private:
  unsigned int nextID = 0;
  std::unordered_map<unsigned int, uint32_t> idToMask;
  std::unordered_map<unsigned int, std::string> idToName;
  std::unordered_map<std::string, unsigned int> nameToID;
  ComponentManager<Transform> transformManager;
  ComponentManager<MeshFilter> filterManager;
  ComponentManager<MeshRenderer> rendererManager;
  ComponentManager<Camera> cameraManager;
  ComponentManager<Light> lightManager;
  template <typename T>
  ComponentManager<T>& GetManager();
  template <typename T>
  ComponentMask GetComponentMask() const;
  std::string GenerateName(const std::string&);
public:
  unsigned int Create(std::string = "");
  void Remove(unsigned int);
  bool Rename(unsigned int, std::string);
  const std::string& GetName(unsigned int);
  size_t GetCount() const;
  template <typename T>
  T& AddComponent(unsigned int);
  IComponent* AddComponent(unsigned int, ComponentID);
  IComponent* AddComponent(unsigned int, const std::string&);
  template <typename... T>
  std::tuple<T&...> AddComponents(unsigned int);
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
  T& GetComponent(unsigned int);
  template <typename T>
  T* GetComponentPtr(unsigned int);
  template <typename T>
  T* GetFirstComponent();
  IComponent* GetComponent(unsigned int, const std::string&);
  template <typename... T>
  std::tuple<T&...> GetComponents(unsigned int);
  std::vector<IComponent*> GetAllComponents(unsigned int);
  std::vector<std::string> GetMissingComponents(unsigned int);
  template <typename... T, typename F>
  void ForEach(F);
  template <typename F>
  void ForAll(F);
};
template <typename T>
T& EntityManager::AddComponent(unsigned int id) {
  auto& manager = GetManager<T>();
  if (idToMask.find(id) == idToMask.end())
    return manager.GetDefault();
  if (HasComponent<T>(id))
    return manager.Get(id);
  idToMask[id] |= GetComponentMask<T>();
  return manager.Add(id);
}
template <typename... T>
std::tuple<T&...> EntityManager::AddComponents(unsigned int id) {
  return std::tie(AddComponent<T>(id)...);
}
template <typename T>
void EntityManager::RemoveComponent(unsigned int id) {
  if (idToMask.find(id) == idToMask.end())
    return;
  idToMask[id] &= ~GetComponentMask<T>();
  auto& manager = GetManager<T>();
  manager.Remove(id);
}
template <typename... T>
void EntityManager::RemoveComponents(unsigned int id) {
  if (idToMask.find(id) == idToMask.end())
    return;
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
  if (idToMask.find(id) == idToMask.end())
    return false;
  auto combinedMask = (GetComponentMask<T>() | ...);
  return (idToMask[id] & combinedMask) == combinedMask;
}
template <typename T>
T& EntityManager::GetComponent(unsigned int id) {
  auto& manager = GetManager<T>();
  return manager.Get(id);
}
template <typename T>
T* EntityManager::GetComponentPtr(unsigned int id) {
  auto& manager = GetManager<T>();
  return manager.GetPtr(id);
}
template <typename... T>
std::tuple<T&...> EntityManager::GetComponents(unsigned int id) {
  return std::tie(GetComponent<T>(id)...);
}
template <typename... T, typename F>
void EntityManager::ForEach(F func) {
  for (const auto& [id, _] : idToMask)
    if (HasComponents<T...>(id)) {
      auto components = GetComponents<T...>(id);
      std::apply([&](T&... args) { func(args...); }, components);
    }
}
template <typename F>
void EntityManager::ForAll(F func) {
  for (const auto& [id, _] : idToMask)
    func(id);
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
inline ComponentManager<Camera>& EntityManager::GetManager() {
  return cameraManager;
}
template <>
inline ComponentManager<Light>& EntityManager::GetManager() {
  return lightManager;
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
template <>
inline ComponentMask EntityManager::GetComponentMask<Camera>() const {
  return CameraMask;
}
template <>
inline ComponentMask EntityManager::GetComponentMask<Light>() const {
  return LightMask;
}
template <>
inline Transform* EntityManager::GetFirstComponent<Transform>() {
  return transformManager.GetFirst();
}
template <>
inline MeshFilter* EntityManager::GetFirstComponent<MeshFilter>() {
  return filterManager.GetFirst();
}
template <>
inline MeshRenderer* EntityManager::GetFirstComponent<MeshRenderer>() {
  return rendererManager.GetFirst();
}
template <>
inline Camera* EntityManager::GetFirstComponent<Camera>() {
  return cameraManager.GetFirst();
}
template <>
inline Light* EntityManager::GetFirstComponent<Light>() {
  return lightManager.GetFirst();
}
