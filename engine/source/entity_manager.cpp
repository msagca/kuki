#include <component/camera.hpp>
#include <component/component.hpp>
#include <component/light.hpp>
#include <component/mesh_filter.hpp>
#include <component/mesh_renderer.hpp>
#include <component/transform.hpp>
#include <entity_manager.hpp>
#include <string>
#include <vector>
EntityManager::~EntityManager() {
  for (auto& [type, manager] : managers)
    delete manager;
  names.Clear();
}
unsigned int EntityManager::Create(std::string& name) {
  names.Insert(name);
  idToName[nextID] = name;
  ids.insert(nextID);
  nameToID[idToName[nextID]] = nextID;
  idToMask[nextID] = 0;
  return nextID++;
}
void EntityManager::DeleteRecords(unsigned int id) {
  auto it = idToName.find(id);
  if (it == idToName.end())
    return;
  names.Delete(it->second);
  nameToID.erase(it->second);
  ids.erase(id);
  idToMask.erase(id);
  idToName.erase(id);
  idToChildren.erase(id);
  idToParent.erase(id);
}
void EntityManager::Delete(unsigned int id) {
  if (ids.find(id) == ids.end())
    return;
  RemoveAllComponents(id);
  ForEachChild(id, [&](unsigned int childID) {
    Delete(childID);
  });
  DeleteRecords(id);
}
bool EntityManager::Rename(unsigned int id, std::string& name) {
  auto it = idToName.find(id);
  if (it == idToName.end())
    return false;
  names.Delete(it->second);
  nameToID.erase(it->second);
  names.Insert(name);
  idToName[id] = name;
  nameToID[name] = id;
  return true;
}
const std::string& EntityManager::GetName(unsigned int id) const {
  static const std::string emptyString = "";
  auto it = idToName.find(id);
  if (it != idToName.end())
    return it->second;
  return emptyString;
}
int EntityManager::GetID(const std::string& name) {
  auto it = nameToID.find(name);
  if (it != nameToID.end())
    return it->second;
  return -1;
}
void EntityManager::AddChild(unsigned int parent, unsigned int child) {
  if (ids.find(parent) == ids.end() || ids.find(child) == ids.end())
    return;
  if (idToChildren.find(parent) == idToChildren.end())
    idToChildren[nextID] = {};
  idToChildren[nextID].insert(child);
  idToParent[child] = parent;
}
void EntityManager::RemoveChild(unsigned int parent, unsigned int child) {
  if (ids.find(parent) == ids.end() || ids.find(child) == ids.end())
    return;
  auto it = idToChildren.find(parent);
  if (it == idToChildren.end())
    return;
  it->second.erase(child);
  if (it->second.empty())
    idToChildren.erase(it->first);
  idToParent.erase(child);
}
bool EntityManager::HasChildren(unsigned int id) const {
  auto it = idToChildren.find(id);
  if (it == idToChildren.end())
    return false;
  return it->second.size() > 0;
}
bool EntityManager::HasParent(unsigned int id) const {
  auto it = idToParent.find(id);
  return it != idToParent.end();
}
int EntityManager::GetParent(unsigned int id) const {
  auto it = idToParent.find(id);
  if (it == idToParent.end())
    return -1;
  return it->second;
}
size_t EntityManager::GetCount() const {
  return ids.size();
}
IComponent* EntityManager::AddComponent(unsigned int id, ComponentID componentID) {
  switch (componentID) {
  case ComponentID::Camera:
    return AddComponent<Camera>(id);
  case ComponentID::Light:
    return AddComponent<Light>(id);
  case ComponentID::MeshFilter:
    return AddComponent<MeshFilter>(id);
  case ComponentID::MeshRenderer:
    return AddComponent<MeshRenderer>(id);
  case ComponentID::Transform:
    return AddComponent<Transform>(id);
  default:
    return nullptr;
  }
}
IComponent* EntityManager::AddComponent(unsigned int id, const std::string& name) {
  ComponentID componentID;
  if (name == "Camera")
    componentID = ComponentID::Camera;
  else if (name == "Light")
    componentID = ComponentID::Light;
  else if (name == "MeshFilter")
    componentID = ComponentID::MeshFilter;
  else if (name == "MeshRenderer")
    componentID = ComponentID::MeshRenderer;
  else if (name == "Transform")
    componentID = ComponentID::Transform;
  else
    return nullptr;
  return AddComponent(id, componentID);
}
void EntityManager::RemoveComponent(unsigned int id, ComponentID componentID) {
  switch (componentID) {
  case ComponentID::Camera:
    RemoveComponent<Camera>(id);
    break;
  case ComponentID::Light:
    RemoveComponent<Light>(id);
    break;
  case ComponentID::MeshFilter:
    RemoveComponent<MeshFilter>(id);
    break;
  case ComponentID::MeshRenderer:
    RemoveComponent<MeshRenderer>(id);
    break;
  case ComponentID::Transform:
    RemoveComponent<Transform>(id);
    break;
  default:
    break;
  }
}
void EntityManager::RemoveComponent(unsigned int id, const std::string& name) {
  ComponentID componentID;
  if (name == "Camera")
    componentID = ComponentID::Camera;
  else if (name == "Light")
    componentID = ComponentID::Light;
  else if (name == "MeshFilter")
    componentID = ComponentID::MeshFilter;
  else if (name == "MeshRenderer")
    componentID = ComponentID::MeshRenderer;
  else if (name == "Transform")
    componentID = ComponentID::Transform;
  else
    return;
  return RemoveComponent(id, componentID);
}
void EntityManager::RemoveAllComponents(unsigned int id) {
  auto it = idToMask.find(id);
  if (it == idToMask.end())
    return;
  for (auto& [type, manager] : managers)
    manager->Remove(id);
  it->second = 0;
}
IComponent* EntityManager::GetComponent(unsigned int id, const std::string& name) {
  if (name == "Transform")
    return GetComponent<Transform>(id);
  else if (name == "MeshFilter")
    return GetComponent<MeshFilter>(id);
  else if (name == "MeshRenderer")
    return GetComponent<MeshRenderer>(id);
  else if (name == "Camera")
    return GetComponent<Camera>(id);
  else if (name == "Light")
    return GetComponent<Light>(id);
  else
    return nullptr;
}
std::vector<IComponent*> EntityManager::GetAllComponents(unsigned int id) {
  std::vector<IComponent*> components;
  for (auto& [type, manager] : managers)
    if (manager->Has(id))
      components.emplace_back(manager->GetBase(id));
  return components;
}
std::vector<std::string> EntityManager::GetMissingComponents(unsigned int id) {
  std::vector<std::string> components;
  if (!HasComponent<Transform>(id))
    components.emplace_back("Transform");
  if (!HasComponent<MeshFilter>(id))
    components.emplace_back("MeshFilter");
  if (!HasComponent<MeshRenderer>(id))
    components.emplace_back("MeshRenderer");
  if (!HasComponent<Camera>(id))
    components.emplace_back("Camera");
  if (!HasComponent<Light>(id))
    components.emplace_back("Light");
  return components;
}
