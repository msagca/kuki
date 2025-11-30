#pragma once
#include <component.hpp>
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
  virtual IComponent& AddBase(const ID) = 0;
  virtual void Remove(const ID) = 0;
  virtual bool Has(const ID) = 0;
  virtual IComponent* GetBase(const ID) = 0;
  virtual void Sort() = 0;
  virtual void Update() = 0;
};
template <typename T>
class ComponentManager final : public IComponentManager {
private:
  std::vector<T> components;
  std::unordered_map<ID, size_t> entityToComponent;
  std::vector<ID> componentToEntity;
  size_t inactiveCount{}; // TODO: to reclaim some memory, shrink the array if inactive count gets too high
public:
  size_t ActiveCount();
  size_t InactiveCount();
  T& Add(const ID);
  IComponent& AddBase(const ID) override;
  void Remove(const ID) override;
  bool Has(const ID) override;
  T* Get(const ID);
  /// @brief Casts the component pointer to IComponent pointer
  /// @param Entity Id
  /// @return An IComponent pointer
  IComponent* GetBase(const ID) override;
  T* GetFirst();
  void Sort() override;
  void Update() override;
  template <typename F>
  void ForEach(F&&);
};
template <typename T>
size_t ComponentManager<T>::ActiveCount() {
  return components.size() - inactiveCount;
}
template <typename T>
size_t ComponentManager<T>::InactiveCount() {
  return inactiveCount;
}
template <typename T>
T& ComponentManager<T>::Add(const ID id) {
  if (auto it = entityToComponent.find(id); it != entityToComponent.end())
    return components[it->second];
  auto componentId = components.size();
  if (inactiveCount > 0) {
    componentId = ActiveCount();
    inactiveCount--;
  } else
    components.emplace_back();
  entityToComponent.insert({id, componentId});
  componentToEntity.push_back(id);
  return components[componentId];
}
template <typename T>
IComponent& ComponentManager<T>::AddBase(const ID id) {
  return static_cast<IComponent&>(Add(id));
}
template <typename T>
void ComponentManager<T>::Remove(const ID id) {
  auto it = entityToComponent.find(id);
  if (it == entityToComponent.end())
    return;
  auto componentId = it->second;
  auto lastId = ActiveCount() - 1;
  if (componentId != lastId) {
    // FIXME: clean up GPU resources associated with a removed component
    std::swap(components[componentId], components[lastId]);
    std::swap(componentToEntity[componentId], componentToEntity[lastId]);
    entityToComponent[componentToEntity[componentId]] = componentId;
  }
  entityToComponent.erase(id);
  componentToEntity.pop_back();
  inactiveCount++;
  if constexpr (std::is_same_v<T, Transform>)
    // TODO: implement a partial sort function
    Sort();
}
template <typename T>
bool ComponentManager<T>::Has(const ID id) {
  return entityToComponent.find(id) != entityToComponent.end();
}
template <typename T>
T* ComponentManager<T>::Get(const ID id) {
  if (auto it = entityToComponent.find(id); it != entityToComponent.end())
    return &components[it->second];
  return nullptr;
}
template <typename T>
IComponent* ComponentManager<T>::GetBase(const ID id) {
  return static_cast<IComponent*>(Get(id));
}
template <typename T>
T* ComponentManager<T>::GetFirst() {
  return ActiveCount() > 0 ? &components.front() : nullptr;
}
template <typename T>
template <typename F>
void ComponentManager<T>::ForEach(F&& func) {
  auto func_ = std::forward<F>(func);
  for (auto i = 0; i < ActiveCount(); i++) {
    auto id = componentToEntity[i];
    func_(id, &components[i]);
  }
}
template <typename T>
void ComponentManager<T>::Sort() {}
template <typename T>
void ComponentManager<T>::Update() {}
template <>
inline void ComponentManager<Transform>::Sort() {
  auto count = ActiveCount();
  if (count == 0)
    return;
  std::vector<Transform> components_;
  std::unordered_map<ID, size_t> entityToComponent_;
  std::vector<ID> componentToEntity_;
  components_.reserve(count);
  entityToComponent_.reserve(count);
  componentToEntity_.reserve(count);
  std::stack<size_t> parents;
  for (auto i = 0; i < count; ++i) {
    auto entityId = componentToEntity[i];
    if (entityToComponent_.find(entityId) != entityToComponent_.end())
      // skip if entity has been processed
      continue;
    auto parentId = components[i].parent;
    while (parentId.IsValid()) {
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
      auto componentId = parents.top();
      auto entityId = componentToEntity[componentId];
      componentToEntity_.push_back(entityId);
      auto componentId_ = components_.size();
      entityToComponent_.insert({entityId, componentId_});
      auto& component = components[componentId];
      components_.push_back(component);
      parents.pop();
    }
    componentToEntity_.push_back(entityId);
    auto componentId_ = components_.size();
    entityToComponent_.insert({entityId, componentId_});
    auto& component = components[i];
    components_.push_back(component);
  }
  components = std::move(components_);
  entityToComponent = std::move(entityToComponent_);
  componentToEntity = std::move(componentToEntity_);
  inactiveCount = 0;
}
template <>
inline void ComponentManager<Transform>::Update() {
  auto count = ActiveCount();
  if (count == 0)
    return;
  for (auto i = 0; i < count; ++i) {
    auto& transform = components[i];
    Transform* parentTransform = nullptr;
    auto parentId = transform.parent;
    if (auto it = entityToComponent.find(parentId); it != entityToComponent.end())
      parentTransform = &components[it->second];
    if (parentTransform && parentTransform->dirty)
      transform.dirty = true;
    if (transform.dirty)
      transform.Update(parentTransform);
  }
  for (auto& c : components)
    c.dirty = false;
}
} // namespace kuki
