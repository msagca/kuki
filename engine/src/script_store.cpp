#include <algorithm>
#include <cassert>
#include <script_store.hpp>
namespace kuki {
auto ScriptStore::Clear() -> void {
  inactiveCount = 0;
  entityToComponents.clear();
  componentToEntity.clear();
  components.clear();
}
auto ScriptStore::Remove(const EntityID id) -> bool {
  return Remove<Script>(id);
}
auto ScriptStore::Remove(const EntityID id, const std::type_index type) -> bool {
  auto scripts = entityToComponents.equal_range(id);
  std::vector<size_t> indices;
  for (auto it = scripts.first; it != scripts.second; ++it)
    if (components[it->second]->GetTypeIndex() == type)
      indices.push_back(it->second);
  return RemoveIndices(id, indices);
}
auto ScriptStore::RemoveIndices(const EntityID id, const std::vector<size_t> &indices) -> bool {
  if (indices.empty())
    return false;
  for (size_t i = 0; i < indices.size(); ++i) {
    auto index = indices[i];
    const auto last = ActiveCount() - 1 - i;
    if (index != last) {
      std::swap(components[index], components[last]);
      const auto otherId = componentToEntity[last];
      std::swap(componentToEntity[index], componentToEntity[last]);
      auto otherScripts = entityToComponents.equal_range(otherId);
      for (auto it2 = otherScripts.first; it2 != otherScripts.second; ++it2)
        if (it2->second == last)
          it2->second = index;
    }
    components[last].reset();
    componentToEntity.pop_back();
    inactiveCount++;
  }
  auto remaining = entityToComponents.equal_range(id);
  for (auto it = remaining.first; it != remaining.second;)
    if (std::find(indices.begin(), indices.end(), it->second) != indices.end())
      it = entityToComponents.erase(it);
    else
      ++it;
  return true;
}
auto ScriptStore::ActiveCount() const -> size_t {
  assert(components.size() >= inactiveCount && "Inactive count cannot be larger than the size of the components array.");
  return components.size() - inactiveCount;
}
auto ScriptStore::InactiveCount() const -> size_t {
  return inactiveCount;
}
} // namespace kuki
