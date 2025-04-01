#include <component/component.hpp>
#include <component_manager.hpp>
#include <entity_manager.hpp>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <component/mesh_filter.hpp>
EntityManager::~EntityManager() {
  for (const auto& [type, manager] : typeToManager)
    delete manager;
  names.Clear();
}
IComponentManager* EntityManager::GetManager(std::type_index type) {
  auto it = typeToManager.find(type);
  if (it == typeToManager.end())
    return nullptr;
  return it->second;
}
IComponentManager* EntityManager::GetManager(const std::string& name) {
  auto it = nameToType.find(name);
  if (it == nameToType.end())
    return nullptr;
  return GetManager(it->second);
}
IComponentManager* EntityManager::GetManager(ComponentId id) {
  auto it = idToType.find(id);
  if (it == idToType.end())
    return nullptr;
  return GetManager(it->second);
}
unsigned int EntityManager::Create(std::string& name) {
  names.Insert(name);
  idToName[nextId] = name;
  ids.insert(nextId);
  nameToId[idToName[nextId]] = nextId;
  return nextId++;
}
void EntityManager::DeleteRecords(unsigned int id) {
  auto it = idToName.find(id);
  if (it == idToName.end())
    return;
  names.Delete(it->second);
  nameToId.erase(it->second);
  ids.erase(id);
  idToName.erase(id);
  idToChildren.erase(id);
  idToParent.erase(id);
  octree.Delete(id);
}
void EntityManager::Delete(unsigned int id) {
  if (ids.find(id) == ids.end())
    return;
  RemoveAllComponents(id);
  ForEachChild(id, [this](unsigned int childId) {
    Delete(childId);
  });
  DeleteRecords(id);
}
void EntityManager::Delete(const std::string& name) {
  auto it = nameToId.find(name);
  if (it == nameToId.end())
    return;
  Delete(it->second);
}
void EntityManager::DeleteAll() {
  for (const auto id : ids)
    RemoveAllComponents(id);
  names.Clear();
  ids.clear();
  nameToId.clear();
  idToName.clear();
  idToChildren.clear();
  idToParent.clear();
}
void EntityManager::DeleteAll(const std::string& prefix) {
  names.ForEach(prefix, [this](const std::string& name) {
    Delete(name);
  });
}
bool EntityManager::Rename(unsigned int id, std::string& name) {
  auto it = idToName.find(id);
  if (it == idToName.end())
    return false;
  names.Delete(it->second);
  nameToId.erase(it->second);
  names.Insert(name);
  idToName[id] = name;
  nameToId[name] = id;
  return true;
}
const std::string& EntityManager::GetName(unsigned int id) const {
  static const std::string emptyString = "";
  auto it = idToName.find(id);
  if (it == idToName.end())
    return emptyString;
  return it->second;
}
int EntityManager::GetId(const std::string& name) {
  auto it = nameToId.find(name);
  if (it == nameToId.end())
    return -1;
  return it->second;
}
bool EntityManager::AddChild(unsigned int parent, unsigned int child) {
  if (ids.find(parent) == ids.end() || ids.find(child) == ids.end())
    return false;
  if (idToChildren.find(parent) == idToChildren.end())
    idToChildren[parent] = {};
  idToChildren[parent].insert(child);
  idToParent[child] = parent;
  return true;
}
void EntityManager::RemoveChild(unsigned int parent, unsigned int child) {
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
IComponent* EntityManager::AddComponent(unsigned int id, ComponentId componentId) {
  if (ids.find(id) == ids.end())
    return nullptr;
  auto manager = GetManager(componentId);
  if (!manager)
    return nullptr;
  return &manager->AddBase(id);
}
IComponent* EntityManager::AddComponent(unsigned int id, const std::string& name) {
  if (ids.find(id) == ids.end())
    return nullptr;
  auto manager = GetManager(name);
  if (!manager)
    return nullptr;
  return &manager->AddBase(id);
}
void EntityManager::RemoveComponent(unsigned int id, ComponentId componentId) {
  auto manager = GetManager(componentId);
  if (!manager)
    return;
  manager->Remove(id);
}
void EntityManager::RemoveComponent(unsigned int id, const std::string& name) {
  auto manager = GetManager(name);
  if (!manager)
    return;
  manager->Remove(id);
}
void EntityManager::RemoveAllComponents(unsigned int id) {
  if (ids.find(id) == ids.end())
    return;
  for (const auto& [type, manager] : typeToManager)
    manager->Remove(id);
}
bool EntityManager::HasComponent(unsigned int id, std::type_index type) {
  auto it = typeToManager.find(type);
  if (it == typeToManager.end())
    return false;
  return it->second->Has(id);
}
IComponent* EntityManager::GetComponent(unsigned int id, const std::string& name) {
  auto manager = GetManager(name);
  if (!manager)
    return nullptr;
  return manager->GetBase(id);
}
std::vector<IComponent*> EntityManager::GetAllComponents(unsigned int id) {
  std::vector<IComponent*> components;
  if (ids.find(id) != ids.end())
    for (const auto& [type, manager] : typeToManager)
      if (manager->Has(id))
        components.emplace_back(manager->GetBase(id));
  return components;
}
std::vector<std::string> EntityManager::GetMissingComponents(unsigned int id) {
  std::vector<std::string> components;
  if (ids.find(id) != ids.end())
    for (const auto& [name, type] : nameToType)
      if (!HasComponent(id, type))
        components.emplace_back(name);
  return components;
}
void EntityManager::UpdateOctree() {
  ForEach<MeshFilter>([this](unsigned int id, MeshFilter* filter) {
    octree.Insert(id, filter->mesh.bounds);
  });
}
