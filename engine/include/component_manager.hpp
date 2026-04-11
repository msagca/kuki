#pragma once
#include <bounding_box.hpp>
#include <camera.hpp>
#include <cassert>
#include <concepts.hpp>
#include <id.hpp>
#include <memory>
#include <script.hpp>
#include <stack>
#include <transform.hpp>
#include <unordered_map>
#include <utility>
#include <vector>
namespace kuki {
class IComponentManager {
public:
  virtual ~IComponentManager() = default;
  virtual auto Remove(const EntityID) -> bool = 0;
protected:
  IComponentManager() = default;
};
template <typename T>
class ComponentManager final : public IComponentManager {
public:
  auto Add(const EntityID) -> T *;
  auto ForEach(this auto &, auto &&) -> void;
  auto Get(this auto &self, const EntityID) -> ConstCorrectPointer<decltype(self), T>;
  auto GetAny(this auto &self) -> ConstCorrectPointer<decltype(self), T>;
  auto Has(const EntityID) const -> bool;
  auto Remove(const EntityID) -> bool override;
  auto Sort() -> void;
  auto Update() -> void;
private:
  size_t inactiveCount{};
  std::unordered_map<EntityID, size_t> entityToComponent;
  std::vector<EntityID> componentToEntity;
  std::vector<T> components;
  auto ActiveCount() const -> size_t;
  auto InactiveCount() const -> size_t;
};
template <typename T>
auto ComponentManager<T>::ActiveCount() const -> size_t {
  assert(components.size() >= inactiveCount && "Inactive count cannot be larger than the size of the components array.");
  // TODO: make sure this doesn't wrap around to a huge number (if `inactiveCount` is larger than the size somehow)
  return components.size() - inactiveCount;
}
template <typename T>
auto ComponentManager<T>::Add(const EntityID id) -> T * {
  if (!id)
    return nullptr;
  if (auto it = entityToComponent.find(id); it != entityToComponent.end())
    return &components[it->second];
  auto componentId = components.size();
  if (inactiveCount > 0) {
    componentId = ActiveCount();
    inactiveCount--;
  } else
    components.emplace_back();
  componentToEntity.push_back(id);
  entityToComponent.emplace(id, componentId);
  return &components[componentId];
}
template <typename T>
auto ComponentManager<T>::ForEach(this auto &self, auto &&func) -> void {
  if constexpr (std::is_const_v<std::remove_reference_t<decltype(self)>>) {
    for (auto i = 0; i < self.ActiveCount(); ++i) {
      const auto id = self.componentToEntity[i];
      auto &component = self.components[i];
      func(id, &component);
    }
  } else {
    std::vector<EntityID> ids{self.componentToEntity.begin(), self.componentToEntity.begin() + self.ActiveCount()};
    for (const auto &id : ids)
      if (auto component = self.Get(id))
        func(id, component);
  }
}
template <typename T>
auto ComponentManager<T>::Get(this auto &self, const EntityID id) -> ConstCorrectPointer<decltype(self), T> {
  if (auto it = self.entityToComponent.find(id); it != self.entityToComponent.end())
    return &self.components[it->second];
  return nullptr;
}
template <typename T>
auto ComponentManager<T>::GetAny(this auto &self) -> ConstCorrectPointer<decltype(self), T> {
  if (self.ActiveCount() > 0)
    return &self.components.front();
  return nullptr;
}
template <typename T>
auto ComponentManager<T>::Has(const EntityID id) const -> bool {
  return entityToComponent.find(id) != entityToComponent.end();
}
template <typename T>
auto ComponentManager<T>::InactiveCount() const -> size_t {
  return inactiveCount;
}
template <typename T>
auto ComponentManager<T>::Remove(const EntityID id) -> bool {
  auto it = entityToComponent.find(id);
  if (it == entityToComponent.end())
    return false;
  auto index = it->second;
  auto last = ActiveCount() - 1;
  if (index != last) {
    std::swap(components[index], components[last]);
    const auto otherId = componentToEntity[last];
    std::swap(componentToEntity[index], componentToEntity[last]);
    entityToComponent[otherId] = index;
  }
  entityToComponent.erase(id);
  componentToEntity.pop_back(); // TODO: optimize this
  inactiveCount++;
  if constexpr (std::is_same_v<T, Transform>)
    // TODO: implement a partial sort function
    Sort();
  return true;
}
template <typename T>
auto ComponentManager<T>::Sort() -> void {}
template <>
inline auto ComponentManager<Transform>::Sort() -> void {
  // FIXME: some variables need to be renamed for clarity
  const auto count = ActiveCount();
  if (count == 0)
    return;
  std::vector<Transform> components_;
  std::unordered_map<EntityID, size_t> entityToComponent_;
  std::vector<EntityID> componentToEntity_;
  components_.reserve(count);
  entityToComponent_.reserve(count);
  componentToEntity_.reserve(count);
  std::stack<size_t> parents;
  for (auto i = 0; i < count; ++i) {
    const auto entityId = componentToEntity[i];
    if (entityToComponent_.find(entityId) != entityToComponent_.end())
      // skip if entity has been processed
      continue;
    auto parentId = components[i].parent;
    while (parentId) {
      if (entityToComponent_.find(parentId) != entityToComponent_.end())
        // skip if parent has been processed
        break;
      if (auto it = entityToComponent.find(parentId); it != entityToComponent.end()) {
        parents.push(it->second);
        parentId = components[it->second].parent;
      } else // TODO: if parent ID is valid, then this is unexpected — throw an exception maybe
        break;
    }
    while (!parents.empty()) {
      const auto componentId = parents.top();
      const auto entityId = componentToEntity[componentId];
      componentToEntity_.push_back(entityId);
      const auto componentId_ = components_.size();
      entityToComponent_.insert({entityId, componentId_});
      auto &component = components[componentId];
      components_.push_back(component);
      parents.pop();
    }
    componentToEntity_.push_back(entityId);
    const auto componentId_ = components_.size();
    entityToComponent_.insert({entityId, componentId_});
    auto &component = components[i];
    components_.push_back(component);
  }
  components = std::move(components_);
  entityToComponent = std::move(entityToComponent_);
  componentToEntity = std::move(componentToEntity_);
  inactiveCount = 0;
}
template <typename T>
auto ComponentManager<T>::Update() -> void {}
template <>
inline auto ComponentManager<BoundingBox>::Update() -> void {}
template <>
inline auto ComponentManager<Camera>::Update() -> void {
  const auto count = ActiveCount();
  if (count == 0)
    return;
  for (auto i = 0; i < count; ++i) {
    auto &camera = components[i];
    camera.Update();
  }
}
template <>
inline auto ComponentManager<Transform>::Update() -> void {
  const auto count = ActiveCount();
  if (count == 0)
    return;
  for (auto i = 0; i < count; ++i) {
    auto &transform = components[i];
    const auto parentId = transform.parent;
    Transform *parentTransform = nullptr;
    if (auto it = entityToComponent.find(parentId); it != entityToComponent.end())
      parentTransform = &components[it->second];
    transform.dirty |= parentTransform && parentTransform->dirty;
    if (transform.dirty)
      transform.Update(parentTransform);
  }
  for (auto &c : components)
    c.dirty = false;
}
template <>
class ComponentManager<Script> final : public IComponentManager {
public:
  template <IsScript T>
  auto Add(const EntityID) -> T *;
  auto ForEach(this auto &, auto &&) -> void;
  template <IsScript T>
  auto Get(this auto &self, const EntityID) -> ConstCorrectPointer<decltype(self), T>;
  auto GetAll(this auto &self, const EntityID) -> std::vector<ConstCorrectPointer<decltype(self), Script>>;
  template <IsScript T>
  auto Has(const EntityID) const -> bool;
  template <IsScript T>
  auto Remove(const EntityID) -> bool;
  auto Remove(const EntityID) -> bool override;
  auto Sort() -> void;
private:
  size_t inactiveCount{};
  // NOTE: this specialization stores components as pointers to avoid object slicing; it also allows multiple script components per entity
  std::unordered_multimap<EntityID, size_t> entityToComponents;
  std::vector<EntityID> componentToEntity;
  std::vector<std::unique_ptr<Script>> components;
  auto ActiveCount() const -> size_t;
  auto InactiveCount() const -> size_t;
};
template <IsScript T>
auto ComponentManager<Script>::Add(const EntityID id) -> T * {
  if (!id)
    return nullptr;
  if constexpr (std::is_same_v<Script, T>)
    // Cannot construct base `Script` - caller must provide a concrete derived type.
    return nullptr;
  auto scripts = entityToComponents.equal_range(id);
  for (auto it = scripts.first; it != scripts.second; ++it)
    // check if the entity already has the same script
    if (components[it->second]->Is<T>())
      return static_cast<T *>(components[it->second].get());
  auto componentId = components.size();
  if (inactiveCount > 0) {
    componentId = ActiveCount();
    inactiveCount--;
  } else
    components.emplace_back(std::make_unique<T>());
  componentToEntity.push_back(id);
  entityToComponents.emplace(id, componentId);
  return static_cast<T *>(components[componentId].get());
}
auto ComponentManager<Script>::ForEach(this auto &self, auto &&func) -> void {
  if constexpr (std::is_const_v<std::remove_reference_t<decltype(self)>>) {
    for (auto i = 0; i < self.ActiveCount(); ++i) {
      const auto id = self.componentToEntity[i];
      auto *component = self.components[i].get();
      func(id, component);
    }
  } else {
    std::vector<EntityID> ids{self.componentToEntity.begin(), self.componentToEntity.begin() + self.ActiveCount()};
    for (const auto &id : ids)
      if (auto component = self.Get(id))
        func(id, component);
  }
}
template <IsScript T>
auto ComponentManager<Script>::Get(this auto &self, const EntityID id) -> ConstCorrectPointer<decltype(self), T> {
  auto scripts = self.entityToComponents.equal_range(id);
  if constexpr (std::is_same_v<Script, T>)
    // if type is `Script`, return any script
    if (auto it = scripts.first; it != scripts.second)
      return self.components[it->second].get();
  for (auto it = scripts.first; it != scripts.second; ++it)
    if (self.components[it->second]->template Is<T>())
      return static_cast<ConstCorrectPointer<decltype(self), T>>(self.components[it->second].get());
  return nullptr;
}
auto ComponentManager<Script>::GetAll(this auto &self, const EntityID id) -> std::vector<ConstCorrectPointer<decltype(self), Script>> {
  std::vector<ConstCorrectPointer<decltype(self), Script>> scriptsCopy;
  auto scripts = self.entityToComponents.equal_range(id);
  for (auto it = scripts.first; it != scripts.second; ++it)
    scriptsCopy.push_back(self.components[it->second].get());
  return scriptsCopy;
}
template <IsScript T>
auto ComponentManager<Script>::Has(const EntityID id) const -> bool {
  auto scripts = entityToComponents.equal_range(id);
  if constexpr (std::is_same_v<Script, T>)
    if (auto it = scripts.first; it != scripts.second)
      return true; // entity has a script
  for (auto it = scripts.first; it != scripts.second; ++it)
    if (components[it->second]->template Is<T>())
      return true; // entity has the specific script
  return false;
}
template <IsScript T>
auto ComponentManager<Script>::Remove(const EntityID id) -> bool {
  auto scripts = entityToComponents.equal_range(id);
  const auto count = std::distance(scripts.first, scripts.second);
  if (count == 0)
    return false;
  std::vector<size_t> indices;
  indices.reserve(count);
  for (auto it = scripts.first; it != scripts.second; ++it)
    if constexpr (std::is_same_v<Script, T>)
      indices.push_back(it->second); // remove all scripts
    else if (components[it->second]->Is<T>())
      indices.push_back(it->second); // remove the specific script
  for (auto i = 0; i < indices.size(); ++i) {
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
    componentToEntity.pop_back();
    inactiveCount++;
  }
  entityToComponents.erase(id);
  return true;
}
inline auto ComponentManager<Script>::Remove(const EntityID id) -> bool {
  return Remove<Script>(id);
}
inline auto ComponentManager<Script>::Sort() -> void {
  // TODO: sort scripts based on priority, or use a data structure that stores them sorted
}
inline auto ComponentManager<Script>::ActiveCount() const -> size_t {
  assert(components.size() >= inactiveCount && "Inactive count cannot be larger than the size of the components array.");
  // TODO: make sure this doesn't wrap around to a huge number (if `inactiveCount` is larger than the size somehow)
  return components.size() - inactiveCount;
}
inline auto ComponentManager<Script>::InactiveCount() const -> size_t {
  return inactiveCount;
}
} // namespace kuki
