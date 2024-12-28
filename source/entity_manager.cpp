#include <chrono>
#include <component_types.hpp>
#include <entity_manager.hpp>
#include <string>
#include <vector>
unsigned int EntityManager::Create(std::string name) {
  if (name.size() > 0) {
    auto entityName = name;
    if (nameToID.find(name) != nameToID.end())
      entityName = GenerateName(name);
    idToName[nextID] = entityName;
  } else
    idToName[nextID] = GenerateName("Entity#");
  nameToID[idToName[nextID]] = nextID;
  idToMask[nextID] = 0;
  return nextID++;
}
std::string EntityManager::GenerateName(const std::string& name) {
  auto now = std::chrono::high_resolution_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
  std::hash<std::string> hashFunc;
  auto hash = hashFunc(name + std::to_string(timestamp));
  return name + std::to_string(hash);
}
void EntityManager::Remove(unsigned int id) {
  RemoveAllComponents(id);
  idToMask.erase(id);
  nameToID.erase(idToName[id]);
  idToName.erase(id);
}
bool EntityManager::Rename(unsigned int id, std::string name) {
  if (idToName.find(id) == idToName.end() || name.size() == 0 || nameToID.find(name) != nameToID.end())
    return false;
  nameToID.erase(idToName[id]);
  nameToID[name] = id;
  idToName[id] = name;
  return true;
}
const std::string& EntityManager::GetName(unsigned int id) {
  static const std::string emptyString = "";
  auto it = idToName.find(id);
  if (it != idToName.end())
    return it->second;
  return emptyString;
}
size_t EntityManager::GetCount() const {
  return idToMask.size();
}
IComponent* EntityManager::AddComponent(unsigned int id, ComponentID componentID) {
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
IComponent* EntityManager::AddComponent(unsigned int id, const std::string& name) {
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
void EntityManager::RemoveComponent(unsigned int id, ComponentID componentID) {
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
  default:
    break;
  }
}
void EntityManager::RemoveComponent(unsigned int id, const std::string& name) {
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
void EntityManager::RemoveAllComponents(unsigned int id) {
  if (idToMask.find(id) == idToMask.end())
    return;
  transformManager.Remove(id);
  filterManager.Remove(id);
  rendererManager.Remove(id);
  cameraManager.Remove(id);
  lightManager.Remove(id);
  idToMask[id] = 0;
}
IComponent* EntityManager::GetComponent(unsigned int id, const std::string& name) {
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
std::vector<IComponent*> EntityManager::GetAllComponents(unsigned int id) {
  std::vector<IComponent*> components;
  if (HasComponent<Transform>(id))
    components.push_back(transformManager.GetPtr(id));
  if (HasComponent<MeshFilter>(id))
    components.push_back(filterManager.GetPtr(id));
  if (HasComponent<MeshRenderer>(id))
    components.push_back(rendererManager.GetPtr(id));
  if (HasComponent<Camera>(id))
    components.push_back(cameraManager.GetPtr(id));
  if (HasComponent<Light>(id))
    components.push_back(lightManager.GetPtr(id));
  return components;
}
std::vector<std::string> EntityManager::GetMissingComponents(unsigned int id) {
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
