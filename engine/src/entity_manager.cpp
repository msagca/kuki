#include <component.hpp>
#include <component_cloner.hpp>
#include <component_type.hpp>
#include <entity_manager.hpp>
#include <id.hpp>
#include <string>
#include <transform.hpp>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
namespace kuki {
auto EntityManager::AddChild(const EntityID parent, const EntityID child, bool keepWorld) -> bool {
  if (!IsEntity(parent) || !IsEntity(child))
    return false;
  if (idToChildren.find(parent) == idToChildren.end())
    idToChildren[parent] = {};
  rootEntities.erase(child);
  AddComponent<Transform>(child);
  AddComponent<Transform>(parent);
  auto *childTransform = GetComponent<Transform>(child);
  auto *parentTransform = GetComponent<Transform>(parent);
  childTransform->parent = parent;
  childTransform->Reparent(parentTransform, keepWorld);
  idToChildren[parent].insert(child);
  idToParent[child] = parent;
  RebuildTransformOrder(); // TODO: replace this with a partial rebuild
  return true;
}
auto EntityManager::Clear() -> void {
  archetypeRegistry.Clear();
  idToChildren.clear();
  idToLocation.clear();
  idToName.clear();
  idToParent.clear();
  nameToId.clear();
  rootEntities.clear();
  scriptStore.Clear();
  transformUpdateOrder.clear();
}
auto EntityManager::CopyFrom(const EntityManager &other, const EntityID otherId) -> EntityID {
  if (!otherId || !other.IsEntity(otherId))
    return EntityID::Invalid;
  const auto id = Create(other.GetName(otherId));
  auto cloner = ComponentCloner(*this, id);
  other.ForEachComponent(otherId, [&](ConstComponentVariant component) {
    std::visit(cloner, component);
  });
  other.ForEachChild(otherId, [&](const EntityID otherChildId) {
    const auto childId = CopyFrom(other, otherChildId);
    AddChild(id, childId);
  });
  return id;
}
auto EntityManager::CopyTo(const EntityID id, EntityManager &other) const -> EntityID {
  return other.CopyFrom(*this, id);
}
auto EntityManager::Create(std::string name) -> EntityID {
  const auto id = nextId++;
  if (!name.empty()) {
    idToName[id] = name;
    nameToId.insert({std::move(name), id});
  }
  auto *archetype = archetypeRegistry.GetOrCreateArchetype(ComponentMask{});
  archetype->entities.push_back(id);
  idToLocation[id] = {ComponentMask{}, archetype->entities.size() - 1};
  rootEntities.insert(id);
  transformUpdateOrder.push_back(id);
  return id;
}
auto EntityManager::Delete(const EntityID id) -> bool {
  if (forEachDepth > 0) {
    if (!IsEntity(id))
      return false;
    pendingStructuralChanges.push_back([this, id] { Delete(id); });
    return true;
  }
  if (!RemoveAllComponents(id))
    return false;
  if (auto pit = idToParent.find(id); pit != idToParent.end())
    if (auto cit = idToChildren.find(pit->second); cit != idToChildren.end())
      cit->second.erase(id);
  ForEachChild(id, [this](const EntityID childId) {
    Delete(childId);
  });
  if (auto it = idToLocation.find(id); it != idToLocation.end()) {
    const auto row = it->second.row;
    const auto displaced = archetypeRegistry.RemoveEntity(id, it->second);
    idToLocation.erase(id);
    if (displaced)
      idToLocation[displaced].row = row;
  }
  DeleteRecords(id);
  return true;
}
auto EntityManager::GetComponentTypes(const EntityID id) const -> std::vector<ComponentType> {
  std::vector<ComponentType> components;
  if (auto it = idToLocation.find(id); it != idToLocation.end()) {
    const auto &mask = it->second.signature;
    Component::ForEachSetType(mask, [&](const ComponentType type) {
      components.emplace_back(type);
    });
  }
  return components;
}
auto EntityManager::GetCount() const -> size_t {
  return idToLocation.size();
}
auto EntityManager::GetMissingComponentTypes(const EntityID id) const -> std::vector<ComponentType> {
  std::vector<ComponentType> components;
  if (auto it = idToLocation.find(id); it != idToLocation.end()) {
    const auto &mask = it->second.signature;
    Component::ForEachUnsetType(mask, [this, &id, &components](const ComponentType type) {
      components.emplace_back(type);
    });
  }
  return components;
}
auto EntityManager::GetName(const EntityID id) const -> std::string {
  if (auto it = idToName.find(id); it != idToName.end())
    return it->second;
  if (idToLocation.contains(id))
    return id.ToString();
  return "";
}
auto EntityManager::GetID(const std::string &name) const -> EntityID {
  auto ids = nameToId.equal_range(name);
  if (auto it = ids.first; it != ids.second)
    return it->second;
  return EntityID::Invalid;
}
auto EntityManager::GetStructuralGeneration() const -> size_t {
  return structuralGeneration;
}
auto EntityManager::GetParent(const EntityID id) const -> EntityID {
  if (auto it = idToParent.find(id); it != idToParent.end())
    return it->second;
  return EntityID::Invalid;
}
auto EntityManager::HasChildren(const EntityID id) const -> bool {
  if (auto it = idToChildren.find(id); it != idToChildren.end())
    return it->second.size() > 0;
  return false;
}
auto EntityManager::HasParent(const EntityID id) const -> bool {
  return idToParent.find(id) != idToParent.end();
}
auto EntityManager::IsEntity(const EntityID id) const -> bool {
  return idToLocation.find(id) != idToLocation.end();
}
auto EntityManager::IsEntity(const std::string &name) const -> bool {
  auto ids = nameToId.equal_range(name);
  if (auto it = ids.first; it != ids.second)
    return true;
  return false;
}
auto EntityManager::RemoveChild(const EntityID parent, const EntityID child) -> bool {
  if (auto it = idToChildren.find(parent); it != idToChildren.end()) {
    it->second.erase(child);
    if (it->second.empty())
      idToChildren.erase(it->first);
  } else
    return false;
  if (auto *childTransform = GetComponent<Transform>(child); childTransform) {
    childTransform->parent = EntityID::Invalid;
    childTransform->Reparent(nullptr);
  }
  idToParent.erase(child);
  rootEntities.insert(child);
  RebuildTransformOrder();
  return true;
}
auto EntityManager::RemoveAllComponents(const EntityID id) -> bool {
  auto it = idToLocation.find(id);
  if (it == idToLocation.end())
    return false;
  if (forEachDepth > 0) {
    pendingStructuralChanges.push_back([this, id] { RemoveAllComponents(id); });
    return true;
  }
  if (it->second.signature.test(static_cast<size_t>(ComponentType::Script)))
    scriptStore.Remove(id);
  MoveToArchetype(id, ComponentMask{});
  return true;
}
auto EntityManager::RemoveScript(const EntityID id, const std::type_index type) -> bool {
  if (forEachDepth > 0) {
    pendingStructuralChanges.push_back([this, id, type] { RemoveScript(id, type); });
    return false;
  }
  const auto removed = scriptStore.Remove(id, type);
  if (removed && !scriptStore.Has<Script>(id)) {
    auto it = idToLocation.find(id);
    if (it != idToLocation.end() && it->second.signature.test(static_cast<size_t>(ComponentType::Script))) {
      auto newMask = it->second.signature;
      newMask.reset(static_cast<size_t>(ComponentType::Script));
      MoveToArchetype(id, newMask);
    }
  }
  return removed;
}
auto EntityManager::Rename(const EntityID id, std::string nameNew) -> bool {
  if (auto it = idToName.find(id); it != idToName.end()) {
    const auto &nameOld = it->second;
    auto ids = nameToId.equal_range(nameOld);
    for (auto &it2 = ids.first; it2 != ids.second;) {
      if (it2->second == id) {
        it2 = nameToId.erase(it2);
        break;
      } else
        ++it2;
    }
    if (nameNew.empty())
      idToName.erase(it);
    else {
      it->second = nameNew;
      nameToId.insert({std::move(nameNew), id});
    }
    return true;
  } else if (idToLocation.contains(id)) {
    idToName[id] = nameNew;
    nameToId.insert({std::move(nameNew), id});
    return true;
  }
  return false;
}
auto EntityManager::DeleteRecords(const EntityID id) -> void {
  if (auto it = idToName.find(id); it != idToName.end()) {
    const auto &name = it->second;
    auto ids = nameToId.equal_range(name);
    for (auto &it2 = ids.first; it2 != ids.second;) {
      if (it2->second == id) {
        it2 = nameToId.erase(it2);
        break;
      } else
        ++it2;
    }
  }
  idToName.erase(id);
  idToChildren.erase(id);
  idToParent.erase(id);
  rootEntities.erase(id);
}
auto EntityManager::DrainPendingStructuralChanges() const -> void {
  if (pendingStructuralChanges.empty())
    return;
  auto pending = std::move(pendingStructuralChanges);
  pendingStructuralChanges.clear();
  for (auto &change : pending)
    change();
}
auto EntityManager::MoveToArchetype(const EntityID id, const ComponentMask &newMask) -> void {
  auto &location = idToLocation.at(id);
  const auto result = archetypeRegistry.MoveEntity(id, location, newMask);
  location = result.location;
  if (result.displacedEntity)
    idToLocation[result.displacedEntity].row = result.displacedRow;
  ++structuralGeneration;
}
auto EntityManager::RebuildTransformOrder() -> void {
  transformUpdateOrder.clear();
  transformUpdateOrder.reserve(idToLocation.size());
  for (const auto &id : rootEntities)
    AppendTransformOrder(id);
}
void EntityManager::AppendTransformOrder(const EntityID id) {
  transformUpdateOrder.push_back(id);
  if (auto it = idToChildren.find(id); it != idToChildren.end())
    for (const auto &childId : it->second)
      AppendTransformOrder(childId);
}
auto EntityManager::UpdateTransforms() -> void {
  for (const auto &id : transformUpdateOrder) {
    auto *transform = GetComponent<Transform>(id);
    if (!transform)
      continue;
    Transform *parentTransform = nullptr;
    if (auto parentIt = idToParent.find(id); parentIt != idToParent.end())
      parentTransform = GetComponent<Transform>(parentIt->second);
    transform->dirty |= parentTransform && parentTransform->dirty;
    if (transform->dirty)
      transform->Update(parentTransform);
  }
  for (const auto &id : transformUpdateOrder)
    if (auto *transform = GetComponent<Transform>(id))
      transform->dirty = false;
}
} // namespace kuki
