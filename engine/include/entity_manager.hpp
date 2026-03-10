#pragma once
#include <bone_data.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <component_manager.hpp>
#include <component_type.hpp>
#include <concepts.hpp>
#include <gl_material.hpp>
#include <gl_mesh.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <id.hpp>
#include <light.hpp>
#include <material_handle.hpp>
#include <memory>
#include <mesh_handle.hpp>
#include <skybox_handle.hpp>
#include <transform.hpp>
#include <trie.hpp>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace kuki {
class KUKI_ENGINE_API EntityManager {
public:
  // TODO: validate EntityID before performing the following tasks
  auto AddChild(const EntityID, const EntityID, bool = false) -> bool;
  auto AddComponent(const EntityID, const ComponentType) -> void;
  auto CopyFrom(const EntityManager &, const EntityID) -> EntityID;
  auto CopyTo(const EntityID, EntityManager &) const -> EntityID;
  auto Create(std::string = "") -> EntityID;
  auto Delete(const EntityID) -> bool;
  auto DeleteAll() -> void;
  auto GetComponent(const EntityID, const ComponentType) -> ComponentVariant;
  auto GetComponent(const EntityID, const ComponentType) const -> const ComponentVariant;
  auto GetComponentTypes(const EntityID) const -> std::vector<ComponentType>;
  auto GetCount() const -> size_t;
  auto GetMissingComponentTypes(const EntityID) const -> std::vector<ComponentType>;
  auto GetName(const EntityID) const -> std::string;
  auto GetID(const std::string &) const -> EntityID;
  auto GetParent(const EntityID) const -> EntityID;
  auto HasChildren(const EntityID) const -> bool;
  auto HasParent(const EntityID) const -> bool;
  auto IsEntity(const EntityID) const -> bool;
  auto IsEntity(const std::string &) const -> EntityID;
  auto RemoveChild(const EntityID, const EntityID) -> bool;
  auto RemoveComponent(const EntityID, const ComponentType) -> bool;
  auto RemoveAllComponents(const EntityID) -> bool;
  auto Rename(const EntityID, std::string) -> bool;
  auto Update() -> void;
  // templates
  auto ForEachChild(this auto &, const EntityID, auto &&) -> void;
  auto ForEachComponent(this auto &, const EntityID, auto &&) -> void;
  auto ForEachRoot(this auto &, auto &&) -> void;
  template <typename... T>
  auto AddComponent(const EntityID) -> decltype(auto);
  template <typename... T>
  auto ForEach(this auto &, auto &&) -> void;
  template <typename... T>
  auto ForFirst(this auto &, auto &&) -> void;
  template <typename... T>
  auto GetComponent(this auto &, const EntityID) -> decltype(auto);
  template <typename T>
  auto GetFirst(this auto &self) -> ConstCorrectPointer<decltype(self), T>;
  template <typename... T>
  auto HasComponent(const EntityID) const -> bool;
  template <typename... T>
  auto RemoveComponent(const EntityID) -> bool;
  template <typename... T>
  auto SortComponents() -> void;
  template <typename... T>
  auto UpdateComponents() -> void;
private:
  EntityID nextId{0};
  // NOTE: to check for the existance of an entity, search the `idToMask` map; the id may not always be present in the other maps
  std::unordered_map<ComponentMask, std::unordered_set<EntityID>> maskToIdSet;
  std::unordered_map<EntityID, ComponentMask> idToMask;
  std::unordered_map<EntityID, EntityID> idToParent;
  std::unordered_map<EntityID, std::string> idToName;
  std::unordered_map<EntityID, std::unordered_set<EntityID>> idToChildren;
  std::unordered_map<std::type_index, std::unique_ptr<IComponentManager>> typeIndexToManager;
  std::unordered_multimap<std::string, EntityID> nameToId;
  std::unordered_set<ComponentType> registeredTypes;
  std::unordered_set<EntityID> rootEntities;
  void DeleteRecords(const EntityID);
  // templates
  template <typename T>
  auto GetManager(this auto &) -> decltype(auto);
};
auto EntityManager::ForEachChild(this auto &self, const EntityID parent, auto &&func) -> void {
  if (auto it = self.idToChildren.find(parent); it != self.idToChildren.end())
    for (const auto &childId : it->second)
      func(childId);
}
auto EntityManager::ForEachComponent(this auto &self, const EntityID id, auto &&func) -> void {
  if (auto it = self.idToMask.find(id); it != self.idToMask.end()) {
    const auto &mask = it->second;
    Component::ForEachSetType(mask, [&](const ComponentType type) {
      auto component = self.GetComponent(id, type);
      func(component);
    });
  }
}
auto EntityManager::ForEachRoot(this auto &self, auto &&func) -> void {
  for (const auto &id : self.rootEntities)
    func(id);
}
template <typename... T>
auto EntityManager::AddComponent(const EntityID id) -> decltype(auto) {
  static_assert(sizeof...(T) > 0, "`AddComponent` requires at least one type parameter.");
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    auto manager = GetManager<C>();
    if (manager->Has(id))
      return manager->Get(id);
    const auto typeIndex = std::type_index(typeid(C));
    ComponentMask mask{0};
    if (auto it = idToMask.find(id); it != idToMask.end()) {
      mask = it->second;
      if (auto it2 = maskToIdSet.find(it->second); it2 != maskToIdSet.end()) {
        it2->second.erase(id);
        if (it2->second.empty())
          maskToIdSet.erase(it2);
      }
    } else
      return static_cast<C *>(nullptr);
    mask.set(Component::GetBit(typeIndex));
    idToMask[id] = mask;
    maskToIdSet[mask].insert(id);
    return &manager->Add(id);
  } else
    return std::tie(AddComponent<T>(id)...);
}
template <typename... T>
auto EntityManager::ForEach(this auto &self, auto &&func) -> void {
  if constexpr (sizeof...(T) == 0)
    for (const auto &[id, _] : self.idToMask)
      func(id);
  else {
    ComponentMask mask;
    (mask.set(Component::GetBit(typeid(T))), ...);
    for (const auto &[m, s] : self.maskToIdSet)
      if ((mask & m) == mask)
        for (const auto &id : s) {
          auto components = self.template GetComponent<T...>(id);
          if constexpr (sizeof...(T) == 1)
            func(id, components);
          else
            std::apply([&](auto... args) { func(id, args...); }, components);
        }
  }
}
template <typename... T>
auto EntityManager::ForFirst(this auto &self, auto &&func) -> void {
  static_assert(sizeof...(T) > 0, "`ForFirst` requires at least one type parameter.");
  ComponentMask mask;
  (mask.set(Component::GetBit(typeid(T))), ...);
  for (const auto &[m, s] : self.maskToIdSet)
    if ((mask & m) == mask) {
      const auto id = *s.begin();
      auto components = self.template GetComponent<T...>(id);
      if constexpr (sizeof...(T) == 1)
        func(id, components);
      else
        std::apply([&](auto... args) { func(id, args...); }, components);
    }
}
template <typename... T>
auto EntityManager::GetComponent(this auto &self, const EntityID id) -> decltype(auto) {
  static_assert(sizeof...(T) > 0, "`GetComponent` requires at least one type parameter.");
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    if (auto manager = self.template GetManager<C>(); manager)
      return manager->Get(id);
    return static_cast<ConstCorrectPointer<decltype(self), C>>(nullptr);
  } else
    return std::tuple(self.template GetComponent<T>(id)...);
}
template <typename T>
auto EntityManager::GetFirst(this auto &self) -> ConstCorrectPointer<decltype(self), T> {
  if (auto manager = self.template GetManager<T>(); manager)
    return manager->GetAny();
  return nullptr;
}
template <typename... T>
auto EntityManager::HasComponent(const EntityID id) const -> bool {
  ComponentMask mask;
  (mask.set(Component::GetBit(typeid(T))), ...);
  for (const auto &[m, s] : maskToIdSet)
    if ((mask & m) == mask)
      if (s.contains(id))
        return true;
  return false;
}
template <typename... T>
auto EntityManager::RemoveComponent(const EntityID id) -> bool {
  static_assert(sizeof...(T) > 0, "`RemoveComponent` requires at least one type parameter.");
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    if (auto manager = GetManager<C>(); manager)
      if (manager->Has(id)) {
        const auto typeIndex = std::type_index(typeid(C));
        if (auto it = idToMask.find(id); it != idToMask.end()) {
          auto &mask = it->second;
          if (auto it2 = maskToIdSet.find(it->second); it2 != maskToIdSet.end()) {
            it2->second.erase(id);
            if (it2->second.size() == 0)
              maskToIdSet.erase(it2);
          }
          mask.reset(Component::GetBit(typeIndex));
          maskToIdSet[mask].insert(id);
        }
        return manager->Remove(id);
      }
    return false;
  } else
    return (RemoveComponent<T>(id) && ...);
}
template <typename... T>
auto EntityManager::SortComponents() -> void {
  static_assert(sizeof...(T) > 0, "`SortComponents` requires at least one type parameter.");
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    if (auto manager = GetManager<C>(); manager)
      manager->Sort();
  } else
    (SortComponents<T>(), ...);
}
template <typename... T>
auto EntityManager::UpdateComponents() -> void {
  static_assert(sizeof...(T) > 0, "`UpdateComponents` requires at least one type parameter.");
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    if (auto manager = GetManager<C>(); manager)
      manager->Update();
  } else
    (UpdateComponents<T>(), ...);
}
template <typename T>
auto EntityManager::GetManager(this auto &self) -> decltype(auto) {
  const auto typeIndex = std::type_index(typeid(T));
  if (auto it = self.typeIndexToManager.find(typeIndex); it != self.typeIndexToManager.end())
    return static_cast<ConstCorrectPointer<decltype(self), ComponentManager<T>>>(it->second.get());
  if constexpr (std::is_const_v<std::remove_reference_t<decltype(self)>>)
    return static_cast<const ComponentManager<T> *>(nullptr);
  else {
    auto [it, _] = self.typeIndexToManager.emplace(typeIndex, std::make_unique<ComponentManager<T>>());
    self.registeredTypes.insert(Component::GetType(typeIndex));
    return static_cast<ComponentManager<T> *>(it->second.get());
  }
}
} // namespace kuki
