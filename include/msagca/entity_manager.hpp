#pragma once
#include <component_manager.hpp>
#include <component_types.hpp>
#include <functional>
#include <tuple>
#include <format>
enum ComponentID {
  TransformID,
  MeshFilterID,
  MeshRendererID,
  CameraID,
  LightID
};
enum ComponentMask {
  TransformMask = 1 << TransformID,
  MeshFilterMask = 1 << MeshFilterID,
  MeshRendererMask = 1 << MeshRendererID,
  CameraMask = 1 << CameraID,
  LightMask = 1 << LightID
};
class EntityManager {
private:
  unsigned int nextID = 0;
  std::unordered_map<unsigned int, uint32_t> entities; // key: entity ID, value: component mask
  std::unordered_map<unsigned int, std::string> names; // key: entity ID, value: entity name
  ComponentManager<Transform> transformManager;
  ComponentManager<MeshFilter> filterManager;
  ComponentManager<MeshRenderer> rendererManager;
  ComponentManager<Camera> cameraManager;
  ComponentManager<Light> lightManager;
  template <typename T>
  ComponentMask GetComponentMask() const;
public:
  unsigned int CreateEntity(std::string = "");
  void RemoveEntity(unsigned int);
  void RenameEntity(unsigned int, std::string);
  const std::string& GetName(unsigned int);
  const std::string GetNextName() const;
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
  IComponent* GetComponent(unsigned int, const std::string&);
  template <typename... T>
  std::tuple<T&...> GetComponents(unsigned int);
  std::vector<IComponent*> GetAllComponents(unsigned int);
  std::vector<std::string> GetMissingComponents(unsigned int);
  template <typename T>
  ComponentManager<T>& GetManager();
  template <typename... T>
  void ForEach(std::function<void(unsigned int)>);
  template <typename... T, typename F>
  void ForEach(F);
  void ForAll(std::function<void(unsigned int)>);
  template <typename T>
  typename std::vector<T>::const_iterator Begin();
  template <typename T>
  typename std::vector<T>::const_iterator End();
  template <typename T>
  T* GetFirst();
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
  return std::format("Entity.{}", nextID);
}
inline size_t EntityManager::GetCount() const {
  return entities.size();
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
inline IComponent* EntityManager::AddComponent(unsigned int id, ComponentID componentID) {
  switch (componentID) {
  case TransformID:
    return &AddComponent<Transform>(id);
  case MeshFilterID:
    return &AddComponent<MeshFilter>(id);
  case MeshRendererID:
    return &AddComponent<MeshRenderer>(id);
  case CameraID:
    return &AddComponent<Camera>(id);
  case LightID:
    return &AddComponent<Light>(id);
  default:
    return nullptr;
  }
}
inline IComponent* EntityManager::AddComponent(unsigned int id, const std::string& name) {
  ComponentID componentID;
  if (name == "Transform")
    componentID = TransformID;
  else if (name == "MeshFilter")
    componentID = MeshFilterID;
  else if (name == "MeshRenderer")
    componentID = MeshRendererID;
  else if (name == "Camera")
    componentID = CameraID;
  else if (name == "Light")
    componentID = LightID;
  else
    return nullptr;
  return AddComponent(id, componentID);
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
inline void EntityManager::RemoveComponent(unsigned int id, ComponentID componentID) {
  switch (componentID) {
  case TransformID:
    RemoveComponent<Transform>(id);
    break;
  case MeshFilterID:
    RemoveComponent<MeshFilter>(id);
    break;
  case MeshRendererID:
    RemoveComponent<MeshRenderer>(id);
    break;
  case CameraID:
    RemoveComponent<Camera>(id);
    break;
  case LightID:
    RemoveComponent<Light>(id);
    break;
  }
}
inline void EntityManager::RemoveComponent(unsigned int id, const std::string& name) {
  ComponentID componentID;
  if (name == "Transform")
    componentID = TransformID;
  else if (name == "MeshFilter")
    componentID = MeshFilterID;
  else if (name == "MeshRenderer")
    componentID = MeshRendererID;
  else if (name == "Camera")
    componentID = CameraID;
  else if (name == "Light")
    componentID = LightID;
  else
    return;
  return RemoveComponent(id, componentID);
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
  transformManager.RemoveComponent(id);
  filterManager.RemoveComponent(id);
  rendererManager.RemoveComponent(id);
  cameraManager.RemoveComponent(id);
  lightManager.RemoveComponent(id);
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
  else if (name == "Camera")
    return GetComponentPtr<Camera>(id);
  else if (name == "Light")
    return GetComponentPtr<Light>(id);
  else
    return nullptr;
}
template <typename... T>
std::tuple<T&...> EntityManager::GetComponents(unsigned int id) {
  return std::tie(GetComponent<T>(id)...);
}
inline std::vector<IComponent*> EntityManager::GetAllComponents(unsigned int id) {
  std::vector<IComponent*> components;
  if (HasComponent<Transform>(id))
    components.push_back(transformManager.GetComponentPtr(id));
  if (HasComponent<MeshFilter>(id))
    components.push_back(filterManager.GetComponentPtr(id));
  if (HasComponent<MeshRenderer>(id))
    components.push_back(rendererManager.GetComponentPtr(id));
  if (HasComponent<Camera>(id))
    components.push_back(cameraManager.GetComponentPtr(id));
  if (HasComponent<Light>(id))
    components.push_back(lightManager.GetComponentPtr(id));
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
  if (!HasComponent<Camera>(id))
    components.push_back("Camera");
  if (!HasComponent<Light>(id))
    components.push_back("Light");
  return components;
}
template <typename... T>
void EntityManager::ForEach(std::function<void(unsigned int)> func) {
  for (const auto& [id, _] : entities)
    if (HasComponents<T...>(id))
      func(id);
}
template <typename... T, typename F>
void EntityManager::ForEach(F func) {
  for (const auto& [id, _] : entities)
    if (HasComponents<T...>(id)) {
      auto components = GetComponents<T...>(id);
      std::apply([&](T&... args) { func(args...); }, components);
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
inline typename std::vector<Camera>::const_iterator EntityManager::Begin<Camera>() {
  return cameraManager.Begin();
}
template <>
inline typename std::vector<Camera>::const_iterator EntityManager::End<Camera>() {
  return cameraManager.End();
}
template <>
inline typename std::vector<Light>::const_iterator EntityManager::Begin<Light>() {
  return lightManager.Begin();
}
template <>
inline typename std::vector<Light>::const_iterator EntityManager::End<Light>() {
  return lightManager.End();
}
template <>
inline Transform* EntityManager::GetFirst<Transform>() {
  return transformManager.GetFirst();
}
template <>
inline MeshFilter* EntityManager::GetFirst<MeshFilter>() {
  return filterManager.GetFirst();
}
template <>
inline MeshRenderer* EntityManager::GetFirst<MeshRenderer>() {
  return rendererManager.GetFirst();
}
template <>
inline Camera* EntityManager::GetFirst<Camera>() {
  return cameraManager.GetFirst();
}
template <>
inline Light* EntityManager::GetFirst<Light>() {
  return lightManager.GetFirst();
}
