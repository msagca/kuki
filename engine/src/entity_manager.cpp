#include <component/component.hpp>
#include <component_manager.hpp>
#include <entity_manager.hpp>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>
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
IComponentManager* EntityManager::GetManager(ComponentID id) {
  auto it = idToType.find(id);
  if (it == idToType.end())
    return nullptr;
  return GetManager(it->second);
}
unsigned int EntityManager::Create(std::string& name) {
  names.Insert(name);
  idToName[nextID] = name;
  ids.insert(nextID);
  nameToID[idToName[nextID]] = nextID;
  return nextID++;
}
void EntityManager::DeleteRecords(unsigned int id) {
  auto it = idToName.find(id);
  if (it == idToName.end())
    return;
  names.Delete(it->second);
  nameToID.erase(it->second);
  ids.erase(id);
  idToName.erase(id);
  idToChildren.erase(id);
  idToParent.erase(id);
}
void EntityManager::Delete(unsigned int id) {
  if (ids.find(id) == ids.end())
    return;
  RemoveAllComponents(id);
  ForEachChild(id, [this](unsigned int childID) {
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
std::string EntityManager::GetName(unsigned int id) const {
  auto it = idToName.find(id);
  if (it == idToName.end())
    return "";
  return it->second;
}
int EntityManager::GetID(const std::string& name) {
  auto it = nameToID.find(name);
  if (it == nameToID.end())
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
IComponent* EntityManager::AddComponent(unsigned int id, ComponentID componentID) {
  if (ids.find(id) == ids.end())
    return nullptr;
  auto manager = GetManager(componentID);
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
void EntityManager::RemoveComponent(unsigned int id, ComponentID componentID) {
  auto manager = GetManager(componentID);
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
