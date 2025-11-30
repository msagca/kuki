#include <component.hpp>
#include <component_manager.hpp>
#include <component_traits.hpp>
#include <entity_manager.hpp>
#include <id.hpp>
#include <list>
#include <string>
#include <transform.hpp>
#include <typeindex>
#include <unordered_map>
#include <vector>
namespace kuki {
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
IComponentManager* EntityManager::GetManager(ComponentType type) {
  auto it = idToType.find(type);
  if (it == idToType.end())
    return nullptr;
  return GetManager(it->second);
}
ID EntityManager::Create(std::string& name) {
  names.Insert(name);
  auto id = ID::Generate();
  idToName[id] = name;
  ids.insert(id);
  nameToId[idToName[id]] = id;
  return id;
}
void EntityManager::DeleteRecords(const ID id) {
  auto it = idToName.find(id);
  if (it == idToName.end())
    return;
  names.Remove(it->second);
  nameToId.erase(it->second);
  ids.erase(id);
  idToName.erase(id);
  idToChildren.erase(id);
  idToParent.erase(id);
}
void EntityManager::Delete(const ID id) {
  if (ids.find(id) == ids.end())
    return;
  RemoveAllComponents(id);
  ForEachChild(id, [this](const ID childId) {
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
bool EntityManager::Rename(const ID id, std::string& name) {
  auto it = idToName.find(id);
  if (it == idToName.end())
    return false;
  names.Remove(it->second);
  nameToId.erase(it->second);
  names.Insert(name);
  idToName[id] = name;
  nameToId[name] = id;
  return true;
}
bool EntityManager::IsEntity(const ID id) {
  if (ids.find(id) != ids.end())
    return true;
  return false;
}
const std::string& EntityManager::GetName(const ID id) const {
  static const std::string emptyString = "";
  if (auto it = idToName.find(id); it != idToName.end())
    return it->second;
  return emptyString;
}
ID EntityManager::GetId(const std::string& name) {
  if (auto it = nameToId.find(name); it != nameToId.end())
    return it->second;
  return ID::Invalid();
}
bool EntityManager::AddChild(const ID parent, const ID child, bool keepWorld) {
  if (!parent.IsValid())
    return false;
  if (ids.find(parent) == ids.end() || ids.find(child) == ids.end())
    return false;
  if (idToChildren.find(parent) == idToChildren.end())
    idToChildren[parent] = {};
  auto transformManager = GetManager<Transform>();
  auto childTransform = transformManager->Get(child);
  if (!childTransform)
    childTransform = &transformManager->Add(child);
  auto parentTransform = transformManager->Get(parent);
  if (!parentTransform)
    parentTransform = &transformManager->Add(parent);
  childTransform->parent = parent;
  childTransform->Reparent(parentTransform, keepWorld);
  idToChildren[parent].insert(child);
  idToParent[child] = parent;
  transformManager->Sort(); // TODO: replace this with a partial sort function
  return true;
}
void EntityManager::RemoveChild(const ID parent, const ID child) {
  auto it = idToChildren.find(parent);
  if (it == idToChildren.end())
    return;
  it->second.erase(child);
  if (it->second.empty())
    idToChildren.erase(it->first);
  auto transformManager = GetManager<Transform>();
  auto childTransform = transformManager->Get(child);
  if (childTransform) {
    childTransform->parent = ID::Invalid();
    childTransform->Reparent(nullptr);
  }
  idToParent.erase(child);
  transformManager->Sort();
}
bool EntityManager::HasChildren(const ID id) const {
  auto it = idToChildren.find(id);
  if (it == idToChildren.end())
    return false;
  return it->second.size() > 0;
}
bool EntityManager::HasParent(const ID id) const {
  auto it = idToParent.find(id);
  return it != idToParent.end();
}
ID EntityManager::GetParent(const ID id) const {
  auto it = idToParent.find(id);
  if (it == idToParent.end())
    return ID::Invalid();
  return it->second;
}
size_t EntityManager::GetCount() const {
  return ids.size();
}
IComponent* EntityManager::AddComponent(const ID id, ComponentType componentId) {
  if (ids.find(id) == ids.end())
    return nullptr;
  auto manager = GetManager(componentId);
  if (!manager)
    return nullptr;
  return &manager->AddBase(id);
}
IComponent* EntityManager::AddComponent(const ID id, const std::string& name) {
  if (ids.find(id) == ids.end())
    return nullptr;
  auto manager = GetManager(name);
  if (!manager)
    return nullptr;
  return &manager->AddBase(id);
}
void EntityManager::RemoveComponent(const ID id, ComponentType componentId) {
  auto manager = GetManager(componentId);
  if (!manager)
    return;
  manager->Remove(id);
}
void EntityManager::RemoveComponent(const ID id, const std::string& name) {
  auto manager = GetManager(name);
  if (!manager)
    return;
  manager->Remove(id);
}
void EntityManager::RemoveAllComponents(const ID id) {
  if (ids.find(id) == ids.end())
    return;
  for (const auto& [type, manager] : typeToManager)
    manager->Remove(id);
}
bool EntityManager::HasComponent(const ID id, std::type_index type) {
  auto it = typeToManager.find(type);
  if (it == typeToManager.end())
    return false;
  return it->second->Has(id);
}
IComponent* EntityManager::GetComponent(const ID id, ComponentType type) {
  auto manager = GetManager(type);
  if (!manager)
    return nullptr;
  return manager->GetBase(id);
}
IComponent* EntityManager::GetComponent(const ID id, const std::string& name) {
  auto manager = GetManager(name);
  if (!manager)
    return nullptr;
  return manager->GetBase(id);
}
std::vector<IComponent*> EntityManager::GetAllComponents(const ID id) {
  std::vector<IComponent*> components;
  if (ids.find(id) != ids.end())
    for (const auto& [type, manager] : typeToManager)
      if (manager->Has(id))
        components.emplace_back(manager->GetBase(id));
  return components;
}
std::vector<std::string> EntityManager::GetMissingComponents(const ID id) {
  std::vector<std::string> components;
  if (ids.find(id) != ids.end())
    for (const auto& [name, type] : nameToType)
      if (!HasComponent(id, type))
        components.emplace_back(name);
  return components;
}
void EntityManager::Update() {
  for (auto& [_, manager] : typeToManager)
    manager->Update();
}
} // namespace kuki
