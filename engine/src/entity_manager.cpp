#include <algorithm>
#include <bit>
#include <component.hpp>
#include <component_type.hpp>
#include <entity_manager.hpp>
#include <id.hpp>
#include <profiler.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <transform.hpp>
#include <unordered_map>
#include <utility>
#include <vector>
namespace kuki {
namespace {
  /// @brief A signature as the names of its components, for saying which archetype was missing.
  auto Describe(const ComponentMask &mask) -> std::string {
    std::string names;
    for (size_t id = 0; id < MAX_COMPONENTS; ++id) {
      if (!mask[id])
        continue;
      if (!names.empty())
        names += ", ";
      names += id < ENGINE_COMPONENT_COUNT ? Component::GetTypeName(static_cast<ComponentType>(id)) : "game component " + std::to_string(id - FIRST_GAME_COMPONENT);
    }
    return names.empty() ? "nothing" : names;
  }
} // namespace
auto EntityManager::Declare(const ArchetypeSpec &spec) -> void {
  archetypeRegistry.Declare(spec);
}
auto EntityManager::IsDeclared(const ComponentMask &mask) const -> bool {
  return archetypeRegistry.Find(mask).has_value();
}
auto EntityManager::AddChild(const EntityID parent, const EntityID child, bool keepWorld) -> bool {
  if (!IsEntity(parent) || !IsEntity(child))
    return false;
  auto *childTransform = GetComponent<Transform>(child);
  auto *parentTransform = GetComponent<Transform>(parent);
  if (!childTransform || !parentTransform) {
    spdlog::warn("[EntityManager] {} cannot be put under {}: both need a transform", GetName(child), GetName(parent));
    return false;
  }
  rootEntities.erase(child);
  childTransform->parent = parent;
  childTransform->Reparent(parentTransform, keepWorld);
  idToChildren[parent].insert(child);
  idToParent[child] = parent;
  transformOrderDirty = true;
  return true;
}
auto EntityManager::Clear() -> void {
  archetypeRegistry.ClearRows();
  idToChildren.clear();
  idToLocation.clear();
  idToName.clear();
  idToParent.clear();
  nameToId.clear();
  rootEntities.clear();
  transformUpdateOrder.clear();
  transformOrderDirty = false;
  transformCacheValid = false;
  ++structuralGeneration;
}
auto EntityManager::CopyFrom(const EntityManager &other, const EntityID otherId) -> EntityID {
  if (!otherId || !other.IsEntity(otherId))
    return EntityID::Invalid;
  const auto id = Create(other.GetSignature(otherId), other.GetName(otherId));
  if (!id)
    return EntityID::Invalid;
  CopyComponents(other, otherId, id);
  other.ForEachChild(otherId, [&](const EntityID otherChildId) {
    if (const auto childId = CopyFrom(other, otherChildId); childId)
      AddChild(id, childId);
  });
  return id;
}
auto EntityManager::CopyTo(const EntityID id, EntityManager &other) const -> EntityID {
  return other.CopyFrom(*this, id);
}
auto EntityManager::CopyComponents(const EntityManager &other, const EntityID from, const EntityID to) -> ComponentMask {
  const auto source = other.idToLocation.find(from);
  const auto destination = idToLocation.find(to);
  if (source == other.idToLocation.end() || destination == idToLocation.end())
    return {};
  const auto *sourceTable = other.archetypeRegistry.GetTable(source->second.table);
  auto *destinationTable = archetypeRegistry.GetTable(destination->second.table);
  for (size_t id = 0; id < MAX_COMPONENTS; ++id)
    if (const auto *sourceColumn = sourceTable->GetColumn(static_cast<ComponentId>(id)))
      if (auto *destinationColumn = destinationTable->GetColumn(static_cast<ComponentId>(id)))
        destinationColumn->CopyRow(*sourceColumn, source->second.row, destination->second.row);
  // A copied transform still names the parent it had over there, which is not an entity here. The
  // hierarchy is put back by whoever is copying it, with `AddChild`.
  if (auto *transform = GetComponent<Transform>(to); transform && !idToParent.contains(to))
    transform->parent = EntityID::Invalid;
  return sourceTable->signature & ~destinationTable->signature;
}
auto EntityManager::Create(const ComponentMask &mask, std::string name) -> EntityID {
  const auto table = archetypeRegistry.Find(mask);
  if (!table) {
    spdlog::error("[EntityManager] No archetype of {} was declared, so '{}' was not made", Describe(mask), name);
    return EntityID::Invalid;
  }
  const auto id = nextId++;
  if (!name.empty()) {
    idToName[id] = name;
    nameToId.insert({std::move(name), id});
  }
  const auto row = archetypeRegistry.Append(*table, id);
  idToLocation[id] = {*table, row};
  rootEntities.insert(id);
  transformUpdateOrder.push_back(id);
  // A row added to a table can move the whole column, and the transform cache points into it.
  ++structuralGeneration;
  return id;
}
auto EntityManager::Delete(const EntityID id) -> bool {
  if (forEachDepth > 0) {
    if (!IsEntity(id))
      return false;
    pendingStructuralChanges.push_back([this, id] { Delete(id); });
    return true;
  }
  if (!IsEntity(id))
    return false;
  if (auto pit = idToParent.find(id); pit != idToParent.end())
    if (auto cit = idToChildren.find(pit->second); cit != idToChildren.end())
      cit->second.erase(id);
  ForEachChild(id, [this](const EntityID childId) {
    Delete(childId);
  });
  const auto location = idToLocation.at(id);
  const auto displaced = archetypeRegistry.Remove(location);
  idToLocation.erase(id);
  if (displaced)
    idToLocation[displaced].row = location.row;
  DeleteRecords(id);
  ++structuralGeneration;
  return true;
}
auto EntityManager::GetComponentTypes(const EntityID id) const -> std::vector<ComponentType> {
  std::vector<ComponentType> components;
  Component::ForEachSetType(GetSignature(id), [&](const ComponentType type) {
    components.emplace_back(type);
  });
  return components;
}
auto EntityManager::GetCount() const -> size_t {
  return idToLocation.size();
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
auto EntityManager::GetSignature(const EntityID id) const -> ComponentMask {
  if (auto it = idToLocation.find(id); it != idToLocation.end())
    return archetypeRegistry.GetTable(it->second.table)->signature;
  return {};
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
  const auto id = static_cast<size_t>(type);
  auto found = false;
  archetypeRegistry.ForEachTable([&](const ArchetypeTable &table) {
    found = found || (table.signature[id] && !table.entities.empty());
  });
  return found;
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
  // Making an entity both lengthens the order and moves the generation on, and either would be
  // enough: a new row can move the column the cache points into, which is what the generation says,
  // and the cache is then short by one, which is what comparing sizes says. Both are kept because
  // they guard different things -- the size the hierarchy's order, the generation the columns.
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
  // change a world matrix, so everything is recomputed once. This is also what makes a new
  // entity's transform correct without its own flag: making it added a row, which is what
  // brought us here.
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
