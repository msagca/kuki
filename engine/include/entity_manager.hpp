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
#include <script.hpp>
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
  auto AddChild(const EntityID, const EntityID, bool = false) -> bool;
  auto AddComponent(const EntityID, const ComponentType) -> void;
  auto Clear() -> void;
  auto CopyFrom(const EntityManager &, const EntityID) -> EntityID;
  auto CopyTo(const EntityID, EntityManager &) const -> EntityID;
  auto Create(std::string = "") -> EntityID;
  auto Delete(const EntityID) -> bool;
  auto GetComponentTypes(const EntityID) const -> std::vector<ComponentType>;
  auto GetCount() const -> size_t;
  auto GetMissingComponentTypes(const EntityID) const -> std::vector<ComponentType>;
  auto GetName(const EntityID) const -> std::string;
  auto GetID(const std::string &) const -> EntityID;
  auto GetParent(const EntityID) const -> EntityID;
  auto HasChildren(const EntityID) const -> bool;
  auto HasParent(const EntityID) const -> bool;
  auto IsEntity(const EntityID) const -> bool;
  auto IsEntity(const std::string &) const -> bool;
  auto RemoveChild(const EntityID, const EntityID) -> bool;
  auto RemoveComponent(const EntityID, const ComponentType) -> bool;
  auto RemoveAllComponents(const EntityID) -> bool;
  auto Rename(const EntityID, std::string) -> bool;
  // FIXME: do not expose Add/Remove methods to callers of the `ForEach*` methods
  auto ForEachChild(this auto &, const EntityID, auto &&) -> void;
  auto ForEachComponent(this auto &, const EntityID, auto &&) -> void;
  auto ForEachRoot(this auto &, auto &&) -> void;
  auto GetComponent(this auto &self, const EntityID, const ComponentType) -> ConstBasedValue<decltype(self), ConstComponentVariant, ComponentVariant>;
  template <typename... T>
  auto AddComponent(const EntityID) -> decltype(auto);
  template <typename... T>
  auto ForEach(this auto &, auto &&) -> void;
  template <typename... T>
  auto ForFirst(this auto &, auto &&) -> void;
  template <typename T>
  auto GetAny(this auto &self) -> ConstCorrectPointer<decltype(self), T>;
  template <typename... T>
  auto GetComponent(this auto &, const EntityID) -> decltype(auto);
  template <typename... T>
  auto GetComponent(this auto &, const std::string &) -> decltype(auto);
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
auto EntityManager::GetComponent(this auto &self, const EntityID id, const ComponentType type) -> ConstBasedValue<decltype(self), ConstComponentVariant, ComponentVariant> {
  switch (type) {
  case ComponentType::BoneData:
    return self.template GetComponent<BoneData>(id);
  case ComponentType::Camera:
    return self.template GetComponent<Camera>(id);
  case ComponentType::GLBuffer:
    return self.template GetComponent<GLBuffer>(id);
  case ComponentType::GLComputeShader:
    return self.template GetComponent<GLComputeShader>(id);
  case ComponentType::GLLitShader:
    return self.template GetComponent<GLLitShader>(id);
  case ComponentType::GLMaterial:
    return self.template GetComponent<GLMaterial>(id);
  case ComponentType::GLMesh:
    return self.template GetComponent<GLMesh>(id);
  case ComponentType::GLRenderTarget:
    return self.template GetComponent<GLRenderTarget>(id);
  case ComponentType::GLSkybox:
    return self.template GetComponent<GLSkybox>(id);
  case ComponentType::GLTexture:
    return self.template GetComponent<GLTexture>(id);
  case ComponentType::GLUnlitShader:
    return self.template GetComponent<GLUnlitShader>(id);
  case ComponentType::Light:
    return self.template GetComponent<Light>(id);
  case ComponentType::MaterialHandle:
    return self.template GetComponent<MaterialHandle>(id);
  case ComponentType::MeshHandle:
    return self.template GetComponent<MeshHandle>(id);
  case ComponentType::SceneMaterialHandle:
    return self.template GetComponent<SceneMaterialHandle>(id);
  case ComponentType::SceneMeshHandle:
    return self.template GetComponent<SceneMeshHandle>(id);
  case ComponentType::Script: {
    auto scripts = self.template GetComponent<Script>(id);
    if (scripts.empty())
      return {};
    else // TODO: decide what to return here
      return scripts[0];
  }
  case ComponentType::SkyboxHandle:
    return self.template GetComponent<SkyboxHandle>(id);
  case ComponentType::TextureHandle:
    return self.template GetComponent<TextureHandle>(id);
  default: // case ComponentType::Transform:
    return self.template GetComponent<Transform>(id);
  }
}
template <typename... T>
auto EntityManager::AddComponent(const EntityID id) -> decltype(auto) {
  static_assert(sizeof...(T) > 0, "`AddComponent` requires at least one type parameter.");
  if constexpr (sizeof...(T) == 1) {
    using F = std::tuple_element_t<0, std::tuple<T...>>;
    using C = ScriptAwareType<F>;
    auto manager = GetManager<C>();
    if constexpr (std::is_base_of_v<Script, F>) {
      if (!id)
        return static_cast<F *>(nullptr);
      if (manager->template Has<F>(id))
        return manager->template Get<F>(id);
    } else {
      if (!id)
        return static_cast<C *>(nullptr);
      if (manager->Has(id))
        return manager->Get(id);
    }
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
      return static_cast<F *>(nullptr);
    if constexpr (std::is_base_of_v<Script, F>) {
      static_assert(!std::is_same_v<F, Script>, "AddComponent<Script> is invalid; pass a concrete derived script type");
      auto component = manager->template Add<F>(id);
      if (component) {
        mask.set(Component::GetBit(typeIndex));
        idToMask[id] = mask;
        maskToIdSet[mask].insert(id);
      }
      return component;
    } else {
      auto component = manager->Add(id);
      if (component) {
        mask.set(Component::GetBit(typeIndex));
        idToMask[id] = mask;
        maskToIdSet[mask].insert(id);
      }
      return component;
    }
  } else
    return std::make_tuple(AddComponent<T>(id)...);
}
template <typename... T>
auto EntityManager::ForEach(this auto &self, auto &&func) -> void {
  if constexpr (sizeof...(T) == 0)
    for (const auto &[id, _] : self.idToMask)
      func(id);
  else {
    std::vector<EntityID> ids; // NOTE: each ID appears in at most one set
    ComponentMask mask;
    (mask.set(Component::GetBit(typeid(T))), ...);
    const auto hasScript = mask.test(static_cast<int>(ComponentType::Script));
    for (const auto &[m, s] : self.maskToIdSet)
      if ((mask & m) == mask)
        for (const auto &id : s)
          ids.push_back(id);
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    for (const auto &id : ids) {
      auto components = self.template GetComponent<T...>(id);
      if constexpr (sizeof...(T) == 1) {
        if constexpr (std::is_same_v<Script, C>)
          for (auto &component : components)
            func(id, component);
        else
          func(id, components);
      } else
        std::apply([&](auto... args) { func(id, args...); }, components);
    }
  }
}
template <typename... T>
auto EntityManager::ForFirst(this auto &self, auto &&func) -> void {
  EntityID id{};
  ComponentMask mask;
  (mask.set(Component::GetBit(typeid(T))), ...);
  for (const auto &[m, s] : self.maskToIdSet)
    if ((mask & m) == mask && !s.empty()) {
      id = *s.begin();
      break;
    }
  if (!id)
    return;
  auto components = self.template GetComponent<T...>(id);
  if constexpr (sizeof...(T) == 1)
    func(id, components);
  else
    std::apply([&](auto... args) { func(id, args...); }, components);
}
template <typename T>
auto EntityManager::GetAny(this auto &self) -> ConstCorrectPointer<decltype(self), T> {
  if (auto manager = self.template GetManager<T>(); manager)
    return manager->GetAny();
  return nullptr;
}
template <typename... T>
auto EntityManager::GetComponent(this auto &self, const EntityID id) -> decltype(auto) {
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    if (auto manager = self.template GetManager<C>(); manager) {
      if constexpr (std::is_same_v<Script, C>)
        return manager->GetAll(id);
      else if constexpr (std::is_base_of_v<Script, C>)
        return manager->template Get<C>(id);
      else
        return manager->Get(id);
    } else {
      if constexpr (std::is_same_v<Script, C>)
        return std::vector<ConstCorrectPointer<decltype(self), C>>();
      else
        return static_cast<ConstCorrectPointer<decltype(self), C>>(nullptr);
    }
  } else
    return std::make_tuple(self.template GetComponent<T>(id)...);
}
template <typename... T>
auto EntityManager::GetComponent(this auto &self, const std::string &name) -> decltype(auto) {
  EntityID id{};
  auto ids = self.nameToId.equal_range(name);
  if (auto it = ids.first; it != ids.second)
    id = it->second;
  return self.template GetComponent<T...>(id);
}
template <typename... T>
auto EntityManager::HasComponent(const EntityID id) const -> bool {
  // FIXME: `ComponentMask` has a `Script` bit, but it doesn't encode potentially many script derivatives; use `ComponentManager->Has(id)` here
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
    if (auto manager = GetManager<C>(); manager) {
      if constexpr (IsScript<C>) {
        if (manager->template Has<C>(id)) {
          const auto removed = manager->template Remove<C>(id);
          if (!manager->template Has<Script>(id)) {
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
          }
          return removed;
        }
      } else if (manager->Has(id)) {
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
  using C = ScriptAwareType<T>;
  const auto typeIndex = std::type_index(typeid(C));
  if (auto it = self.typeIndexToManager.find(typeIndex); it != self.typeIndexToManager.end())
    return static_cast<ConstCorrectPointer<decltype(self), ComponentManager<C>>>(it->second.get());
  if constexpr (std::is_const_v<std::remove_reference_t<decltype(self)>>)
    return static_cast<const ComponentManager<C> *>(nullptr);
  else {
    auto [it, _] = self.typeIndexToManager.emplace(typeIndex, std::make_unique<ComponentManager<C>>());
    self.registeredTypes.insert(Component::GetType(typeIndex));
    return static_cast<ComponentManager<C> *>(it->second.get());
  }
}
} // namespace kuki
