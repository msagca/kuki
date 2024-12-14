#pragma once
#include <component_manager.hpp>
#include <component_types.hpp>
#include <functional>
#include <tuple>
#include <format>
enum ComponentType {
  TransformType,
  MeshFilterType,
  MeshRendererType,
  PrimitiveType,
  CameraType
};
enum ComponentMask {
  TransformMask = 1 << TransformType,
  MeshFilterMask = 1 << MeshFilterType,
  MeshRendererMask = 1 << MeshRendererType,
  PrimitiveMask = 1 << PrimitiveType,
  CameraMask = 1 << CameraType
};
class EntityManager {
private:
  unsigned int nextID = 0;
  std::unordered_map<unsigned int, uint32_t> entities; // key: entity ID, value: component mask
  std::unordered_map<unsigned int, std::string> names; // key: entity ID, value: entity name
  ComponentManager<Transform> transformManager;
  ComponentManager<MeshFilter> filterManager;
  ComponentManager<MeshRenderer> rendererManager;
  ComponentManager<Primitive> primitiveManager;
  ComponentManager<Camera> cameraManager;
  template <typename T>
  ComponentMask GetComponentMask() const;
public:
  unsigned int CreateEntity(std::string = "");
  void RemoveEntity(unsigned int);
  void RenameEntity(unsigned int, std::string);
  const std::string& GetName(unsigned int);
  const std::string GetNextName() const;
  size_t GetCount() const;
  void CleanUp();
  template <typename T>
  T& AddComponent(unsigned int);
  IComponent* AddComponent(unsigned int, ComponentType);
  IComponent* AddComponent(unsigned int, const std::string&);
  template <typename... T>
  std::tuple<T&...> AddComponents(unsigned int);
  template <typename T>
  void RemoveComponent(unsigned int);
  void RemoveComponent(unsigned int, ComponentType);
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
  IComponent* GetComponent(unsigned int, const std::string&);
  template <typename... T>
  std::tuple<T&...> GetComponents(unsigned int);
  std::vector<IComponent*> GetAllComponents(unsigned int);
  std::vector<std::string> GetMissingComponents(unsigned int);
  template <typename T>
  ComponentManager<T>& GetManager();
  template <typename... T>
  void ForEach(std::function<void(unsigned int)>);
  template <typename... T>
  void ForEach(std::function<void(unsigned int, T&...)>);
  void ForAll(std::function<void(unsigned int)>);
  template <typename T>
  typename std::vector<T>::const_iterator Begin();
  template <typename T>
  typename std::vector<T>::const_iterator End();
};
inline unsigned int EntityManager::CreateEntity(std::string name) {
  entities[nextID] = 0;
  if (name.size() > 0)
    // TODO: check if the name is available
    names[nextID] = name;
  else
    names[nextID] = GetNextName();
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
inline const std::string EntityManager::GetNextName() const {
  return std::format("Entity {}", nextID);
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
  if (entities.find(id) == entities.end())
    return manager.GetDefault();
  if (HasComponent<T>(id))
    return manager.GetComponent(id);
  entities[id] |= GetComponentMask<T>();
  return manager.AddComponent(id);
}
inline IComponent* EntityManager::AddComponent(unsigned int id, ComponentType type) {
  switch (type) {
  case ComponentType::TransformType:
    return &AddComponent<Transform>(id);
    break;
  case ComponentType::MeshFilterType:
    return &AddComponent<MeshFilter>(id);
    break;
  case ComponentType::MeshRendererType:
    return &AddComponent<MeshRenderer>(id);
    break;
  case ComponentType::PrimitiveType:
    return &AddComponent<Primitive>(id);
    break;
  case ComponentType::CameraType:
    return &AddComponent<Camera>(id);
    break;
  default:
    return nullptr;
  }
}
inline IComponent* EntityManager::AddComponent(unsigned int id, const std::string& name) {
  ComponentType type;
  if (name == "Transform")
    type = ComponentType::TransformType;
  else if (name == "MeshFilter")
    type = ComponentType::MeshFilterType;
  else if (name == "MeshRenderer")
    type = ComponentType::MeshRendererType;
  else if (name == "Primitive")
    type = ComponentType::PrimitiveType;
  else if (name == "Camera")
    type = ComponentType::CameraType;
  else
    return nullptr;
  return AddComponent(id, type);
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
inline void EntityManager::RemoveComponent(unsigned int id, ComponentType type) {
  switch (type) {
  case ComponentType::TransformType:
    return RemoveComponent<Transform>(id);
    break;
  case ComponentType::MeshFilterType:
    return RemoveComponent<MeshFilter>(id);
    break;
  case ComponentType::MeshRendererType:
    return RemoveComponent<MeshRenderer>(id);
    break;
  case ComponentType::PrimitiveType:
    return RemoveComponent<Primitive>(id);
    break;
  case ComponentType::CameraType:
    return RemoveComponent<Camera>(id);
    break;
  }
}
inline void EntityManager::RemoveComponent(unsigned int id, const std::string& name) {
  ComponentType type;
  if (name == "Transform")
    type = ComponentType::TransformType;
  else if (name == "MeshFilter")
    type = ComponentType::MeshFilterType;
  else if (name == "MeshRenderer")
    type = ComponentType::MeshRendererType;
  else if (name == "Primitive")
    type = ComponentType::PrimitiveType;
  else if (name == "Camera")
    type = ComponentType::CameraType;
  else
    return;
  return RemoveComponent(id, type);
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
  primitiveManager.RemoveComponent(id);
  cameraManager.RemoveComponent(id);
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
template <typename T>
T* EntityManager::GetComponentPtr(unsigned int id) {
  auto& manager = GetManager<T>();
  return manager.GetComponentPtr(id);
}
inline IComponent* EntityManager::GetComponent(unsigned int id, const std::string& name) {
  if (name == "Transform")
    return GetComponentPtr<Transform>(id);
  else if (name == "MeshFilter")
    return GetComponentPtr<MeshFilter>(id);
  else if (name == "MeshRenderer")
    return GetComponentPtr<MeshRenderer>(id);
  else if (name == "Primitive")
    return GetComponentPtr<Primitive>(id);
  else if (name == "Camera")
    return GetComponentPtr<Camera>(id);
  else
    return nullptr;
}
template <typename... T>
std::tuple<T&...> EntityManager::GetComponents(unsigned int id) {
  return std::tie(GetComponent<T>(id)...);
}
inline std::vector<IComponent*> EntityManager::GetAllComponents(unsigned int id) {
  std::vector<IComponent*> components;
  // TODO: make this a loop
  if (HasComponent<Transform>(id))
    components.push_back(transformManager.GetComponentPtr(id));
  if (HasComponent<MeshFilter>(id))
    components.push_back(filterManager.GetComponentPtr(id));
  if (HasComponent<MeshRenderer>(id))
    components.push_back(rendererManager.GetComponentPtr(id));
  if (HasComponent<Primitive>(id))
    components.push_back(primitiveManager.GetComponentPtr(id));
  if (HasComponent<Camera>(id))
    components.push_back(cameraManager.GetComponentPtr(id));
  return components;
}
inline std::vector<std::string> EntityManager::GetMissingComponents(unsigned int id) {
  std::vector<std::string> components;
  if (!HasComponent<Transform>(id))
    components.push_back("Transform");
  if (!HasComponent<MeshFilter>(id))
    components.push_back("MeshFilter");
  if (!HasComponent<MeshRenderer>(id))
    components.push_back("MeshRenderer");
  if (!HasComponent<Primitive>(id))
    components.push_back("Primitive");
  if (!HasComponent<Camera>(id))
    components.push_back("Camera");
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
inline void EntityManager::ForAll(std::function<void(unsigned int)> func) {
  for (const auto& [id, _] : entities)
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
inline ComponentManager<Primitive>& EntityManager::GetManager() {
  return primitiveManager;
}
template <>
inline ComponentManager<Camera>& EntityManager::GetManager() {
  return cameraManager;
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
inline ComponentMask EntityManager::GetComponentMask<Primitive>() const {
  return PrimitiveMask;
}
template <>
inline ComponentMask EntityManager::GetComponentMask<Camera>() const {
  return CameraMask;
}
template <>
inline typename std::vector<Transform>::const_iterator EntityManager::Begin<Transform>() {
  return transformManager.Begin();
}
template <>
inline typename std::vector<Transform>::const_iterator EntityManager::End<Transform>() {
  return transformManager.End();
}
template <>
inline typename std::vector<MeshFilter>::const_iterator EntityManager::Begin<MeshFilter>() {
  return filterManager.Begin();
}
template <>
inline typename std::vector<MeshFilter>::const_iterator EntityManager::End<MeshFilter>() {
  return filterManager.End();
}
template <>
inline typename std::vector<MeshRenderer>::const_iterator EntityManager::Begin<MeshRenderer>() {
  return rendererManager.Begin();
}
template <>
inline typename std::vector<MeshRenderer>::const_iterator EntityManager::End<MeshRenderer>() {
  return rendererManager.End();
}
template <>
inline typename std::vector<Primitive>::const_iterator EntityManager::Begin<Primitive>() {
  return primitiveManager.Begin();
}
template <>
inline typename std::vector<Primitive>::const_iterator EntityManager::End<Primitive>() {
  return primitiveManager.End();
}
template <>
inline typename std::vector<Camera>::const_iterator EntityManager::Begin<Camera>() {
  return cameraManager.Begin();
}
template <>
inline typename std::vector<Camera>::const_iterator EntityManager::End<Camera>() {
  return cameraManager.End();
}
