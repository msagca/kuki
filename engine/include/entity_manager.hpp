#pragma once
#include <animator.hpp>
#include <archetype_registry.hpp>
#include <bone_data.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <component_type.hpp>
#include <concepts.hpp>
#include <event.hpp>
#include <functional>
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
#include <script_store.hpp>
#include <skeleton.hpp>
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
  auto Clear() -> void;
  auto CopyFrom(const EntityManager &, const EntityID) -> EntityID;
  auto CopyTo(const EntityID, EntityManager &) const -> EntityID;
  auto Create(std::string = "") -> EntityID;
  auto Delete(const EntityID) -> bool;
  auto ForEachChild(this const auto &, const EntityID, auto &&) -> void;
  auto ForEachComponent(this const auto &, const EntityID, auto &&) -> void;
  auto ForEachRoot(this const auto &, auto &&) -> void;
  auto GetComponent(this auto &self, const EntityID, const ComponentType) -> ConstBasedValue<decltype(self), ConstComponentVariant, ComponentVariant>;
  auto GetComponentTypes(const EntityID) const -> std::vector<ComponentType>;
  auto GetCount() const -> size_t;
  auto GetID(const std::string &) const -> EntityID;
  auto GetStructuralGeneration() const -> size_t;
  auto GetMissingComponentTypes(const EntityID) const -> std::vector<ComponentType>;
  auto GetName(const EntityID) const -> std::string;
  auto GetParent(const EntityID) const -> EntityID;
  auto HasChildren(const EntityID) const -> bool;
  auto HasParent(const EntityID) const -> bool;
  auto IsEntity(const EntityID) const -> bool;
  auto IsEntity(const std::string &) const -> bool;
  auto RemoveAllComponents(const EntityID) -> bool;
  auto RemoveChild(const EntityID, const EntityID) -> bool;
  auto RemoveScript(const EntityID, const std::type_index) -> bool;
  auto Rename(const EntityID, std::string) -> bool;
  template <typename... T>
  auto AddComponent(const EntityID) -> decltype(auto);
  template <typename... T>
  auto ForEach(this auto &, auto &&) -> void;
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
  auto UpdateComponents() -> void;
private:
  EntityID nextId{0};
  ArchetypeRegistry archetypeRegistry;
  ScriptStore scriptStore;
  std::unordered_map<EntityID, EntityLocation> idToLocation;
  std::unordered_map<EntityID, EntityID> idToParent;
  std::unordered_map<EntityID, std::string> idToName;
  std::unordered_map<EntityID, std::unordered_set<EntityID>> idToChildren;
  std::unordered_multimap<std::string, EntityID> nameToId;
  std::unordered_set<EntityID> rootEntities;
  std::vector<EntityID> transformUpdateOrder;
  size_t structuralGeneration{};
  mutable size_t forEachDepth{};
  mutable std::vector<std::function<void()>> pendingStructuralChanges;
  template <typename F>
  auto AddComponentImmediate(const EntityID) -> F *;
  void DeleteRecords(const EntityID);
  auto DrainPendingStructuralChanges() const -> void;
  auto MoveToArchetype(const EntityID, const ComponentMask &) -> void;
  auto RebuildTransformOrder() -> void;
  void AppendTransformOrder(const EntityID);
  auto UpdateTransforms() -> void;
};
auto EntityManager::ForEachChild(this const auto &self, const EntityID parent, auto &&func) -> void {
  if (auto it = self.idToChildren.find(parent); it != self.idToChildren.end()) {
    const auto children = it->second;
    for (const auto &childId : children)
      func(childId);
  }
}
auto EntityManager::ForEachComponent(this const auto &self, const EntityID id, auto &&func) -> void {
  if (auto it = self.idToLocation.find(id); it != self.idToLocation.end()) {
    const auto &mask = it->second.signature;
    Component::ForEachSetType(mask, [&](const ComponentType type) {
      auto component = self.GetComponent(id, type);
      func(component);
    });
  }
}
auto EntityManager::ForEachRoot(this const auto &self, auto &&func) -> void {
  const auto roots = self.rootEntities;
  for (const auto &id : roots)
    func(id);
}
auto EntityManager::GetComponent(this auto &self, const EntityID id, const ComponentType type) -> ConstBasedValue<decltype(self), ConstComponentVariant, ComponentVariant> {
  switch (type) {
  case ComponentType::Animator:
    return self.template GetComponent<Animator>(id);
  case ComponentType::BoneData:
    return self.template GetComponent<BoneData>(id);
  case ComponentType::BoundingBox:
    return self.template GetComponent<BoundingBox>(id);
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
  case ComponentType::ModelMaterialHandle:
    return self.template GetComponent<ModelMaterialHandle>(id);
  case ComponentType::ModelMeshHandle:
    return self.template GetComponent<ModelMeshHandle>(id);
  case ComponentType::Script: {
    auto scripts = self.template GetComponent<Script>(id);
    if (scripts.empty())
      return {};
    else // TODO: decide what to return here
      return scripts[0];
  }
  case ComponentType::Skeleton:
    return self.template GetComponent<Skeleton>(id);
  case ComponentType::SkyboxHandle:
    return self.template GetComponent<SkyboxHandle>(id);
  case ComponentType::TextureHandle:
    return self.template GetComponent<TextureHandle>(id);
  default:
    return self.template GetComponent<Transform>(id);
  }
}
template <typename F>
auto EntityManager::AddComponentImmediate(const EntityID id) -> F * {
  if constexpr (std::is_base_of_v<Script, F>) {
    static_assert(!std::is_same_v<F, Script>, "AddComponent<Script> is invalid; pass a concrete derived script type");
    if (!id)
      return nullptr;
    if (scriptStore.Has<F>(id))
      return scriptStore.Get<F>(id);
    auto it = idToLocation.find(id);
    if (it == idToLocation.end())
      return nullptr;
    auto component = scriptStore.Add<F>(id);
    if (component && !it->second.signature.test(static_cast<size_t>(ComponentType::Script))) {
      auto newMask = it->second.signature;
      newMask.set(static_cast<size_t>(ComponentType::Script));
      MoveToArchetype(id, newMask);
    }
    return component;
  } else {
    if (!id)
      return nullptr;
    auto it = idToLocation.find(id);
    if (it == idToLocation.end())
      return nullptr;
    const auto bit = Component::GetBit(typeid(F));
    if (!it->second.signature.test(bit)) {
      auto newMask = it->second.signature;
      newMask.set(bit);
      MoveToArchetype(id, newMask);
    }
    return GetComponent<F>(id);
  }
}
template <typename... T>
auto EntityManager::AddComponent(const EntityID id) -> decltype(auto) {
  static_assert(sizeof...(T) > 0, "`AddComponent` requires at least one type parameter.");
  if constexpr (sizeof...(T) == 1) {
    using F = std::tuple_element_t<0, std::tuple<T...>>;
    if (forEachDepth > 0) {
      pendingStructuralChanges.push_back([this, id] { AddComponentImmediate<F>(id); });
      return static_cast<F *>(nullptr);
    }
    return AddComponentImmediate<F>(id);
  } else
    return std::make_tuple(AddComponent<T>(id)...);
}
template <typename... T>
auto EntityManager::ForEach(this auto &self, auto &&func) -> void {
  static_assert(sizeof...(T) <= 1 || !(std::is_same_v<T, Script> || ...), "ForEach: `Script` cannot be combined with other component types in one query");
  ++self.forEachDepth;
  if constexpr (sizeof...(T) == 0) {
    for (const auto &[id, location] : self.idToLocation)
      func(id);
  } else if constexpr (sizeof...(T) == 1 && std::is_same_v<std::tuple_element_t<0, std::tuple<T...>>, Script>) {
    self.scriptStore.ForEach([&](const EntityID id, auto *script) {
      func(id, script);
    });
  } else {
    ComponentMask mask;
    (mask.set(Component::GetBit(typeid(T))), ...);
    self.archetypeRegistry.ForEachArchetype([&](auto &archetype) {
      if ((mask & archetype.signature) != mask)
        return;
      auto columns = std::make_tuple(archetype.template GetColumn<T>()...);
      const auto rowCount = archetype.entities.size();
      for (size_t row = 0; row < rowCount; ++row) {
        const auto id = archetype.entities[row];
        std::apply([&](auto *...col) { func(id, &col->components[row]...); }, columns);
      }
    });
  }
  --self.forEachDepth;
  if (self.forEachDepth == 0)
    self.DrainPendingStructuralChanges();
}
template <typename T>
auto EntityManager::GetAny(this auto &self) -> ConstCorrectPointer<decltype(self), T> {
  ConstCorrectPointer<decltype(self), T> result = nullptr;
  if constexpr (std::is_base_of_v<Script, T>)
    return result;
  else {
    self.archetypeRegistry.ForEachArchetype([&](auto &archetype) {
      if (result)
        return;
      if (auto *column = archetype.template GetColumn<T>(); column && column->Size() > 0)
        result = &column->components[0];
    });
    return result;
  }
}
template <typename... T>
auto EntityManager::GetComponent(this auto &self, const EntityID id) -> decltype(auto) {
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    if constexpr (std::is_same_v<Script, C>)
      return self.scriptStore.GetAll(id);
    else if constexpr (std::is_base_of_v<Script, C>)
      return self.scriptStore.template Get<C>(id);
    else {
      using PointerType = ConstCorrectPointer<decltype(self), C>;
      auto it = self.idToLocation.find(id);
      if (it == self.idToLocation.end())
        return PointerType(nullptr);
      auto *archetype = self.archetypeRegistry.GetArchetype(it->second.signature);
      auto *column = archetype ? archetype->template GetColumn<C>() : nullptr;
      if (!column)
        return PointerType(nullptr);
      return PointerType(&column->components[it->second.row]);
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
  auto it = idToLocation.find(id);
  if (it == idToLocation.end())
    return false;
  ComponentMask mask;
  (mask.set(Component::GetBit(typeid(T))), ...);
  return (it->second.signature & mask) == mask;
}
template <typename... T>
auto EntityManager::RemoveComponent(const EntityID id) -> bool {
  static_assert(sizeof...(T) > 0, "`RemoveComponent` requires at least one type parameter.");
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    if (forEachDepth > 0) {
      pendingStructuralChanges.push_back([this, id] { RemoveComponent<C>(id); });
      return false;
    }
    if constexpr (IsScript<C>) {
      if (!scriptStore.template Has<C>(id))
        return false;
      const auto removed = scriptStore.template Remove<C>(id);
      if (!scriptStore.Has<Script>(id)) {
        auto it = idToLocation.find(id);
        if (it != idToLocation.end() && it->second.signature.test(static_cast<size_t>(ComponentType::Script))) {
          auto newMask = it->second.signature;
          newMask.reset(static_cast<size_t>(ComponentType::Script));
          MoveToArchetype(id, newMask);
        }
      }
      return removed;
    } else {
      auto it = idToLocation.find(id);
      if (it == idToLocation.end())
        return false;
      const auto bit = Component::GetBit(typeid(C));
      if (!it->second.signature.test(bit))
        return false;
      auto newMask = it->second.signature;
      newMask.reset(bit);
      MoveToArchetype(id, newMask);
      return true;
    }
  } else
    return (RemoveComponent<T>(id) && ...);
}
template <typename... T>
auto EntityManager::UpdateComponents() -> void {
  static_assert(sizeof...(T) > 0, "`UpdateComponents` requires at least one type parameter.");
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    if constexpr (std::is_same_v<C, Transform>)
      UpdateTransforms();
    else if constexpr (std::is_same_v<C, Camera>)
      ForEach<Camera>([](const EntityID, Camera *camera) { camera->Update(); });
  } else
    (UpdateComponents<T>(), ...);
}
} // namespace kuki
