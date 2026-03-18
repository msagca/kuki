#pragma once
#include <cassert>
#include <concepts.hpp>
#include <id.hpp>
#include <stack>
#include <transform.hpp>
#include <unordered_map>
#include <utility>
#include <vector>
namespace kuki {
class IComponentManager {
public:
  virtual ~IComponentManager() = default;
  virtual auto Has(const EntityID) const -> bool = 0;
  virtual auto Remove(const EntityID) -> bool = 0;
  virtual auto Sort() -> void = 0;
  virtual auto Update() -> void = 0;
};
// TODO: remove the constraint on the type, components don't have to extend the `Component` class
template <typename T>
class ComponentManager final : public IComponentManager {
public:
  auto ActiveCount() const -> size_t;
  auto Add(const EntityID) -> T &;
  auto ForEach(this auto &, auto &&) -> void;
  auto Get(this auto &self, const EntityID) -> ConstCorrectPointer<decltype(self), T>;
  auto GetAny(this auto &self) -> ConstCorrectPointer<decltype(self), T>;
  auto Has(const EntityID) const -> bool override;
  auto InactiveCount() const -> size_t;
  auto Remove(const EntityID) -> bool override;
  auto Sort() -> void override;
  auto Update() -> void override;
private:
  size_t inactiveCount{};
  std::unordered_map<EntityID, size_t> entityToComponent;
  std::vector<EntityID> componentToEntity;
  std::vector<T> components;
};
template <typename T>
auto ComponentManager<T>::ActiveCount() const -> size_t {
  assert(components.size() >= inactiveCount && "Inactive count cannot be larger than the size of the components array.");
  // TODO: make sure this doesn't wrap around to a huge number (if `inactiveCount` is larger than the size somehow)
  return components.size() - inactiveCount;
}
template <typename T>
auto ComponentManager<T>::Add(const EntityID id) -> T & {
  if (auto it = entityToComponent.find(id); it != entityToComponent.end())
    return components[it->second];
  auto componentId = components.size();
  if (inactiveCount > 0) {
    componentId = ActiveCount();
    inactiveCount--;
  } else
    components.emplace_back();
  componentToEntity.push_back(id);
  entityToComponent.emplace(id, componentId);
  return components[componentId];
}
template <typename T>
auto ComponentManager<T>::ForEach(this auto &self, auto &&func) -> void {
  if constexpr (std::is_const_v<std::remove_reference_t<decltype(self)>>) {
    for (auto i = 0; i < self.ActiveCount(); ++i) {
      const auto &id = self.componentToEntity[i];
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
  auto componentId = it->second;
  auto lastId = ActiveCount() - 1;
  if (componentId != lastId) {
    std::swap(components[componentId], components[lastId]);
    std::swap(componentToEntity[componentId], componentToEntity[lastId]);
    entityToComponent[componentToEntity[componentId]] = componentId;
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
template <typename T>
auto ComponentManager<T>::Update() -> void {}
#include <component_manager.inl>
} // namespace kuki
