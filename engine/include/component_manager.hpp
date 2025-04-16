#pragma once
#include <component/mesh.hpp>
#include <component/shader.hpp>
#include <component/texture.hpp>
#include <glad/glad.h>
#include <unordered_map>
#include <utility>
#include <vector>
namespace kuki {
class IComponentManager {
public:
  virtual ~IComponentManager() = default;
  virtual IComponent& AddBase(unsigned int) = 0;
  virtual void Remove(unsigned int) = 0;
  virtual bool Has(unsigned int) = 0;
  virtual IComponent* GetBase(unsigned int) = 0;
};
template <typename T>
class ComponentManager final : public IComponentManager {
private:
  std::vector<T> components;
  std::unordered_map<unsigned int, unsigned int> entityToComponent;
  std::vector<unsigned int> componentToEntity;
  unsigned int inactiveCount{}; // TODO: to reclaim some memory, shrink the array if inactive count gets too high
  template <typename F>
  void ForAll(F);
public:
  unsigned int ActiveCount();
  unsigned int InactiveCount();
  T& Add(unsigned int);
  IComponent& AddBase(unsigned int) override;
  void Remove(unsigned int) override;
  bool Has(unsigned int) override;
  T* Get(unsigned int);
  /// @brief Casts the component pointer to IComponent pointer
  /// @param Entity Id
  /// @return An IComponent pointer
  IComponent* GetBase(unsigned int) override;
  T* GetFirst();
  template <typename F>
  void ForEach(F);
};
template <typename T>
unsigned int ComponentManager<T>::ActiveCount() {
  return components.size() - inactiveCount;
}
template <typename T>
unsigned int ComponentManager<T>::InactiveCount() {
  return inactiveCount;
}
template <typename T>
T& ComponentManager<T>::Add(unsigned int id) {
  auto it = entityToComponent.find(id);
  if (it != entityToComponent.end())
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
IComponent& ComponentManager<T>::AddBase(unsigned int id) {
  return static_cast<IComponent&>(Add(id));
}
template <typename T>
void ComponentManager<T>::Remove(unsigned int id) {
  auto it = entityToComponent.find(id);
  if (it == entityToComponent.end())
    return;
  auto componentId = it->second;
  auto lastId = ActiveCount() - 1;
  if (componentId != lastId) {
    std::swap(components[componentId], components[lastId]);
    std::swap(componentToEntity[componentId], componentToEntity[lastId]);
    entityToComponent[componentToEntity[componentId]] = componentId;
  }
  entityToComponent.erase(id);
  componentToEntity.pop_back();
  inactiveCount++;
}
template <typename T>
bool ComponentManager<T>::Has(unsigned int id) {
  return entityToComponent.find(id) != entityToComponent.end();
}
template <typename T>
T* ComponentManager<T>::Get(unsigned int id) {
  auto it = entityToComponent.find(id);
  if (it == entityToComponent.end())
    return nullptr;
  return &components[it->second];
}
template <typename T>
IComponent* ComponentManager<T>::GetBase(unsigned int id) {
  return static_cast<IComponent*>(Get(id));
}
template <typename T>
T* ComponentManager<T>::GetFirst() {
  return ActiveCount() > 0 ? &components.front() : nullptr;
}
template <typename T>
template <typename F>
void ComponentManager<T>::ForEach(F func) {
  for (auto i = 0; i < ActiveCount(); i++)
    func(components[i]);
}
template <typename T>
template <typename F>
void ComponentManager<T>::ForAll(F func) {
  for (const auto& c : components)
    func(c);
}
} // namespace kuki
