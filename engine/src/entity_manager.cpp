#include <algorithm>
#include <bit>
#include <component.hpp>
#include <component_cloner.hpp>
#include <component_type.hpp>
#include <entity_manager.hpp>
#include <id.hpp>
#include <profiler.hpp>
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
  transformOrderDirty = true;
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
  transformOrderDirty = false;
  transformCacheValid = false;
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
auto EntityManager::IsEntity(const EntityID id) const -> bool {
  return idToLocation.find(id) != idToLocation.end();
}
auto EntityManager::IsEntity(const std::string &name) const -> bool {
  auto ids = nameToId.equal_range(name);
  if (auto it = ids.first; it != ids.second)
    return true;
  return false;
}
auto EntityManager::HasComponentAnywhere(const ComponentType type) const -> bool {
  const auto bit = static_cast<size_t>(type);
  if (bit >= ComponentMask{}.size())
    return false;
  for (const auto &[id, location] : idToLocation)
    if (location.signature[bit])
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
  transformOrderDirty = true;
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
  // the id is still sitting in `transformUpdateOrder`, and only a rebuild takes it back out
  transformOrderDirty = true;
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
  KUKI_PROFILE_SCOPE("RebuildTransformOrder");
  transformOrderDirty = false;
  // every cached row is an index into the order being rewritten, so none of them survive it
  transformCacheValid = false;
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
auto EntityManager::IsTransformCacheStale() const -> bool {
  // `Create` appends to the order without touching the generation, which leaves the existing rows
  // valid but the cache short; comparing sizes is what catches that.
  return !transformCacheValid || transformCacheGeneration != structuralGeneration || transformCache.size() != transformUpdateOrder.size();
}
auto EntityManager::RebuildTransformCache() -> void {
  KUKI_PROFILE_SCOPE("RebuildTransformCache");
  const auto count = transformUpdateOrder.size();
  transformCache.assign(count, nullptr);
  transformParent.assign(count, -1);
  transformSubtree.assign(count, 1);
  transformRowById.clear();
  transformRowById.reserve(count);
  for (size_t row = 0; row < count; ++row)
    transformRowById[transformUpdateOrder[row]] = static_cast<uint32_t>(row);
  for (size_t row = 0; row < count; ++row) {
    const auto id = transformUpdateOrder[row];
    transformCache[row] = GetComponent<Transform>(id);
    if (auto parentIt = idToParent.find(id); parentIt != idToParent.end())
      if (auto rowIt = transformRowById.find(parentIt->second); rowIt != transformRowById.end())
        transformParent[row] = static_cast<int32_t>(rowIt->second);
  }
  // A subtree ends where its last descendant's own subtree ends. Walking backwards means a row's
  // span is final before its parent reads it, so one pass is enough; going forwards would settle
  // a parent before the descendants that extend it.
  for (auto row = count; row-- > 0;)
    if (transformParent[row] >= 0) {
      const auto parent = static_cast<size_t>(transformParent[row]);
      transformSubtree[parent] = std::max<uint32_t>(transformSubtree[parent], static_cast<uint32_t>(row - parent) + transformSubtree[row]);
    }
  // A rebuild follows something that moved entities or rewrote the hierarchy, and either can
  // change a world matrix, so everything is recomputed once. This is also what makes a newly
  // added transform correct without its own flag: adding the component moved the entity between
  // archetypes, which is what brought us here.
  transformDirtyBits.assign((count + 63) / 64, ~0ull);
  if (const auto tail = count % 64; tail != 0)
    transformDirtyBits.back() = (1ull << tail) - 1ull;
  transformCacheGeneration = structuralGeneration;
  transformCacheValid = true;
}
auto EntityManager::MarkTransformDirty(const EntityID id) -> void {
  if (IsTransformCacheStale())
    return;
  if (auto rowIt = transformRowById.find(id); rowIt != transformRowById.end())
    transformDirtyBits[rowIt->second >> 6] |= 1ull << (rowIt->second & 63);
}
auto EntityManager::UpdateTransforms() -> void {
  KUKI_PROFILE_SCOPE("UpdateTransforms");
  if (transformOrderDirty)
    RebuildTransformOrder();
  if (IsTransformCacheStale())
    RebuildTransformCache();
  const auto count = transformCache.size();
  size_t row = 0;
  while (row < count) {
    // Everything before `row` is settled, so only the bits above it in this word can still matter.
    // A word with none of them set clears 64 entities without any of them being touched, which is
    // the case an unchanged scene spends its whole pass in.
    const auto word = row >> 6;
    const auto pending = transformDirtyBits[word] & (~0ull << (row & 63));
    if (pending == 0) {
      row = (word + 1) << 6;
      continue;
    }
    // The subtree of a dirty entity is dirty by definition, and preorder puts it in the rows
    // straight after it, parents ahead of children. So the range is recomputed outright and the
    // scan resumes past it: no row asks its parent whether it moved, and no flag inside the range
    // needs reading, set or not.
    const auto first = (word << 6) + static_cast<size_t>(std::countr_zero(pending));
    const auto last = first + transformSubtree[first];
    for (auto member = first; member < last; ++member)
      if (auto *transform = transformCache[member]) {
        const auto parent = transformParent[member];
        transform->Update(parent >= 0 ? transformCache[parent] : nullptr);
      }
    row = last;
  }
  std::fill(transformDirtyBits.begin(), transformDirtyBits.end(), 0ull);
}
} // namespace kuki
