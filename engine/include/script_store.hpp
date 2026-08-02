#pragma once
#include <concepts.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <memory>
#include <script.hpp>
#include <typeindex>
#include <unordered_map>
#include <vector>
namespace kuki {
class KUKI_ENGINE_API ScriptStore {
public:
  template <IsScript T>
  auto Add(const EntityID) -> T *;
  auto Clear() -> void;
  auto ForEach(this auto &, auto &&) -> void;
  template <IsScript T>
  auto Get(this auto &self, const EntityID) -> decltype(auto);
  auto GetAll(this auto &, const EntityID) -> decltype(auto);
  template <IsScript T>
  auto Has(const EntityID) const -> bool;
  auto Remove(const EntityID) -> bool;
  auto Remove(const EntityID, const std::type_index) -> bool;
  template <IsScript T>
  auto Remove(const EntityID) -> bool;
private:
  size_t inactiveCount{};
  std::unordered_multimap<EntityID, size_t> entityToComponents;
  std::vector<EntityID> componentToEntity;
  std::vector<std::unique_ptr<Script>> components;
  auto ActiveCount() const -> size_t;
  auto InactiveCount() const -> size_t;
  auto RemoveIndices(const EntityID, const std::vector<size_t> &) -> bool;
};
template <IsScript T>
auto ScriptStore::Add(const EntityID id) -> T * {
  if (!id)
    return nullptr;
  if constexpr (std::is_same_v<Script, T>)
    return nullptr;
  auto scripts = entityToComponents.equal_range(id);
  for (auto &it = scripts.first; it != scripts.second; ++it)
    if (components[it->second]->Is<T>())
      return static_cast<T *>(components[it->second].get());
  auto componentId = components.size();
  if (inactiveCount > 0) {
    componentId = ActiveCount();
    inactiveCount--;
    components[componentId] = std::make_unique<T>();
  } else
    components.emplace_back(std::make_unique<T>());
  componentToEntity.push_back(id);
  entityToComponents.emplace(id, componentId);
  return static_cast<T *>(components[componentId].get());
}
auto ScriptStore::ForEach(this auto &self, auto &&func) -> void {
  for (size_t i = 0; i < self.ActiveCount(); ++i) {
    const auto id = self.componentToEntity[i];
    auto *component = self.components[i].get();
    func(id, component);
  }
}
template <IsScript T>
auto ScriptStore::Get(this auto &self, const EntityID id) -> decltype(auto) {
  auto scripts = self.entityToComponents.equal_range(id);
  if constexpr (std::is_same_v<Script, T>) {
    if (auto it = scripts.first; it != scripts.second)
      return self.components[it->second].get();
  } else
    for (auto it = scripts.first; it != scripts.second; ++it)
      if (self.components[it->second]->template Is<T>())
        return static_cast<ConstCorrectPointer<decltype(self), T>>(self.components[it->second].get());
  return ConstCorrectPointer<decltype(self), T>(nullptr);
}
auto ScriptStore::GetAll(this auto &self, const EntityID id) -> decltype(auto) {
  std::vector<ConstCorrectPointer<decltype(self), Script>> scriptsCopy;
  auto scripts = self.entityToComponents.equal_range(id);
  for (auto it = scripts.first; it != scripts.second; ++it)
    scriptsCopy.push_back(self.components[it->second].get());
  return scriptsCopy;
}
template <IsScript T>
auto ScriptStore::Has(const EntityID id) const -> bool {
  auto scripts = entityToComponents.equal_range(id);
  if constexpr (std::is_same_v<Script, T>)
    if (auto it = scripts.first; it != scripts.second)
      return true;
  for (auto &it = scripts.first; it != scripts.second; ++it)
    if (components[it->second]->template Is<T>())
      return true;
  return false;
}
template <IsScript T>
auto ScriptStore::Remove(const EntityID id) -> bool {
  auto scripts = entityToComponents.equal_range(id);
  std::vector<size_t> indices;
  for (auto &it = scripts.first; it != scripts.second; ++it)
    if constexpr (std::is_same_v<Script, T>)
      indices.push_back(it->second);
    else if (components[it->second]->Is<T>())
      indices.push_back(it->second);
  return RemoveIndices(id, indices);
}
} // namespace kuki
