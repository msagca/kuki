#include <asset_manager.hpp>
#include <chrono>
#include <component_types.hpp>
#include <entity_manager.hpp>
#include <string>
#include <vector>
EntityManager::EntityManager(AssetManager& assetManager)
  : assetManager(assetManager) {
}
unsigned int EntityManager::Create(std::string name) {
  if (name.size() > 0) {
    auto& entityName = name;
    if (nameToID.find(name) != nameToID.end())
      entityName = GenerateName(name);
    idToName[nextID] = entityName;
  } else
    idToName[nextID] = GenerateName("Entity#");
  nameToID[idToName[nextID]] = nextID;
  idToMask[nextID] = 0;
  idToChildren.insert({nextID, {}});
  return nextID++;
}
int EntityManager::Spawn(const std::string& name) {
  auto assetID = assetManager.GetID(name);
  if (assetID < 0)
    return -1;
  auto entityID = Create(name);
  auto components = assetManager.GetAllComponents(assetID);
  for (auto c : components)
    if (auto t = dynamic_cast<Transform*>(c)) {
      auto transform = AddComponent<Transform>(entityID);
      *transform = *t;
    } else if (auto m = dynamic_cast<Mesh*>(c)) {
      auto filter = AddComponent<MeshFilter>(entityID);
      filter->mesh = *m;
    } else if (auto m = dynamic_cast<Material*>(c)) {
      auto renderer = AddComponent<MeshRenderer>(entityID);
      renderer->material = *m;
    }
  assetManager.ForEachChild(assetID, [&](unsigned int id, const std::string& name) {
    auto childID = Spawn(name);
    AddChild(entityID, childID);
  });
  return entityID;
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
  idToChildren.erase(id);
  idToParent.erase(id);
}
bool EntityManager::Rename(unsigned int id, std::string name) {
  if (idToName.find(id) == idToName.end() || name.size() == 0 || nameToID.find(name) != nameToID.end())
    return false;
  nameToID.erase(idToName[id]);
  nameToID[name] = id;
  idToName[id] = name;
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
  auto it = idToChildren.find(parent);
  if (it == idToChildren.end())
    return;
  it->second.insert(child);
  idToParent[child] = parent;
}
void EntityManager::RemoveChild(unsigned int parent, unsigned int child) {
  auto it = idToChildren.find(parent);
  if (it == idToChildren.end())
    return;
  it->second.erase(child);
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
  if (it == idToParent.end())
    return false;
  return true;
}
int EntityManager::GetParent(unsigned int id) const {
  auto it = idToParent.find(id);
  if (it == idToParent.end())
    return -1;
  return it->second;
}
size_t EntityManager::GetCount() const {
  return idToMask.size();
}
IComponent* EntityManager::AddComponent(unsigned int id, ComponentID componentID) {
  switch (componentID) {
  case ComponentID::TransformID:
    return AddComponent<Transform>(id);
  case ComponentID::MeshFilterID:
    return AddComponent<MeshFilter>(id);
  case ComponentID::MeshRendererID:
    return AddComponent<MeshRenderer>(id);
  case ComponentID::CameraID:
    return AddComponent<Camera>(id);
  case ComponentID::LightID:
    return AddComponent<Light>(id);
  default:
    return nullptr;
  }
}
IComponent* EntityManager::AddComponent(unsigned int id, const std::string& name) {
  ComponentID componentID;
  if (name == "Transform")
    componentID = ComponentID::TransformID;
  else if (name == "MeshFilter")
    componentID = ComponentID::MeshFilterID;
  else if (name == "MeshRenderer")
    componentID = ComponentID::MeshRendererID;
  else if (name == "Camera")
    componentID = ComponentID::CameraID;
  else if (name == "Light")
    componentID = ComponentID::LightID;
  else
    return nullptr;
  return AddComponent(id, componentID);
}
void EntityManager::RemoveComponent(unsigned int id, ComponentID componentID) {
  switch (componentID) {
  case ComponentID::TransformID:
    RemoveComponent<Transform>(id);
    break;
  case ComponentID::MeshFilterID:
    RemoveComponent<MeshFilter>(id);
    break;
  case ComponentID::MeshRendererID:
    RemoveComponent<MeshRenderer>(id);
    break;
  case ComponentID::CameraID:
    RemoveComponent<Camera>(id);
    break;
  case ComponentID::LightID:
    RemoveComponent<Light>(id);
    break;
  default:
    break;
  }
}
void EntityManager::RemoveComponent(unsigned int id, const std::string& name) {
  ComponentID componentID;
  if (name == "Transform")
    componentID = ComponentID::TransformID;
  else if (name == "MeshFilter")
    componentID = ComponentID::MeshFilterID;
  else if (name == "MeshRenderer")
    componentID = ComponentID::MeshRendererID;
  else if (name == "Camera")
    componentID = ComponentID::CameraID;
  else if (name == "Light")
    componentID = ComponentID::LightID;
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
  if (HasComponent<Transform>(id))
    components.emplace_back(transformManager.Get(id));
  if (HasComponent<MeshFilter>(id))
    components.emplace_back(filterManager.Get(id));
  if (HasComponent<MeshRenderer>(id))
    components.emplace_back(rendererManager.Get(id));
  if (HasComponent<Camera>(id))
    components.emplace_back(cameraManager.Get(id));
  if (HasComponent<Light>(id))
    components.emplace_back(lightManager.Get(id));
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
