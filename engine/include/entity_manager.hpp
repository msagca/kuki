#pragma once
#include <animator.hpp>
#include <archetype_registry.hpp>
#include <bone_data.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <component_type.hpp>
#include <concepts.hpp>
#include <cstdint>
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
/// @brief Marks a component a query reads where it exists, rather than one it filters on.
///
/// `ForEach<MeshHandle, Transform, Optional<BoundingBox>>` visits every entity carrying a handle
/// and a transform, and hands the callback a bounds pointer that is null for those without one.
///
/// The point is where the component is read from. A required type narrows the archetypes a query
/// visits, so asking for one an entity might not have would silently skip that entity; the usual
/// way around that is to fetch it by id inside the loop, which costs a hash lookup per entity.
/// Naming it optional instead keeps the archetype set wide and still reads the component straight
/// out of its column, which is the whole reason the storage is laid out in columns.
template <typename T>
struct Optional {};
/// @brief Splits a query parameter into the component it names and whether it was optional.
template <typename T>
struct QueryTraits {
  using Type = T;
  static constexpr auto optional = false;
};
template <typename T>
struct QueryTraits<Optional<T>> {
  using Type = T;
  static constexpr auto optional = true;
};
/// @brief Sets a query parameter's bit in the mask that selects archetypes, unless it is optional.
template <typename T>
auto SetRequiredBit(ComponentMask &mask) -> void {
  if constexpr (!QueryTraits<T>::optional)
    mask.set(Component::GetBit(typeid(typename QueryTraits<T>::Type)));
}
class KUKI_ENGINE_API EntityManager {
public:
  /// @brief Parents one entity to another, optionally preserving the child's world transform.
  ///
  /// Only marks the update order stale rather than rebuilding it. Instantiating a model calls this
  /// once per node, and a rebuild walks the whole scene, so rebuilding eagerly made importing cost
  /// time quadratic in the node count — a few thousand nodes took seconds, spent almost entirely
  /// on rewriting an order that the next call would rewrite again.
  ///
  /// @return True when the link was established.
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
  auto IsEntity(const EntityID) const -> bool;
  auto IsEntity(const std::string &) const -> bool;
  /// @brief Whether any entity at all carries this component. What enforces `IsSceneSingleton`.
  ///
  /// Reads the archetype signatures rather than fetching the component, because the question is
  /// whether one exists and not what is in it, and every entity's signature is already to hand.
  auto HasComponentAnywhere(const ComponentType) const -> bool;
  auto RemoveAllComponents(const EntityID) -> bool;
  auto RemoveChild(const EntityID, const EntityID) -> bool;
  /// @brief Flags an entity's transform, and with it everything below it, for recomputation.
  ///
  /// The way to say a transform changed. `Transform` carries no dirty bit of its own, so writing
  /// to `position`, `rotation` or `scale` without calling this leaves the change unapplied until
  /// something else forces an update.
  ///
  /// Costs one lookup, paid per write rather than per entity per frame, which is the trade the
  /// packed flags exist to make: a scene writes to far fewer transforms than it owns.
  ///
  /// Does nothing when the cache is already stale, because whatever invalidated it will have the
  /// next update rebuild and mark everything anyway. Adding or removing a component does that, so
  /// a freshly added transform needs no call here.
  auto MarkTransformDirty(const EntityID) -> void;
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
  /// @brief Entities in depth-first preorder, so a parent always precedes its children.
  ///
  /// A flat list rather than a walk of the hierarchy, so a frame's transform update is one linear
  /// pass.
  ///
  /// Preorder gives a second guarantee the update relies on: every subtree occupies a CONTIGUOUS
  /// range. That is what lets a dirty entity be resolved as a range rather than by asking each
  /// entity in turn whether its parent moved. Sibling order is unspecified, since the children are
  /// held in an unordered set, but neither guarantee depends on it.
  std::vector<EntityID> transformUpdateOrder;
  /// @brief Whether the hierarchy has changed since the order was last built.
  ///
  /// Set by anything that reparents or deletes, and paid off once, lazily, at the next transform
  /// update. Batching matters because structural changes arrive in bursts: a model import is one
  /// burst of `AddChild` calls, and collapsing them into a single rebuild is the whole point.
  bool transformOrderDirty{};
  /// @brief Every entity's transform, in update order, or null where it has none.
  ///
  /// Held so the update pass costs no lookups at all. Finding a transform by id means hashing
  /// into `idToLocation` and then indexing an archetype column, and the pass used to do that
  /// three times an entity to answer a question about one bit.
  ///
  /// Pointers into archetype columns, so anything that moves an entity between archetypes
  /// invalidates them; `transformCacheGeneration` is what notices.
  std::vector<Transform *> transformCache;
  /// @brief Each entry's parent as an index into `transformCache`, or -1 for a root.
  ///
  /// The direct parent, matching what the update has always used: an entity whose parent carries
  /// no transform composes against nothing rather than against its grandparent.
  std::vector<int32_t> transformParent;
  /// @brief How many consecutive entries each entry's subtree spans, itself included.
  ///
  /// Only meaningful because the order is preorder. Marking one entity dirty implies its whole
  /// subtree, and this is what turns that implication into a range the pass can walk directly.
  std::vector<uint32_t> transformSubtree;
  /// @brief Where each entity sits in the update order, for `MarkTransformDirty`.
  std::unordered_map<EntityID, uint32_t> transformRowById;
  /// @brief One bit per entry, set when that entity's transform needs recomputing.
  ///
  /// Packed and kept here rather than as a `bool` on `Transform`, because the pass reads far more
  /// flags than it acts on and a flag on the component costs a whole cache line to reach. Packed,
  /// a scene's entire dirty state is a few hundred bytes and one `uint64` clears 64 entities from
  /// consideration at once, so an unchanged scene is skipped without touching a component.
  ///
  /// Bits at or past `transformCache.size()` are never set, so the scan can trust a whole word.
  std::vector<uint64_t> transformDirtyBits;
  /// @brief The structural generation `transformCache` was built against.
  size_t transformCacheGeneration{};
  /// @brief Whether the cache has been built at all since the last thing that invalidated it.
  bool transformCacheValid{};
  size_t structuralGeneration{};
  mutable size_t forEachDepth{};
  mutable std::vector<std::function<void()>> pendingStructuralChanges;
  template <typename F>
  auto AddComponentImmediate(const EntityID) -> F *;
  void DeleteRecords(const EntityID);
  auto DrainPendingStructuralChanges() const -> void;
  auto MoveToArchetype(const EntityID, const ComponentMask &) -> void;
  auto RebuildTransformOrder() -> void;
  auto RebuildTransformCache() -> void;
  auto IsTransformCacheStale() const -> bool;
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
  case ComponentType::DXMaterial:
    return self.template GetComponent<DXMaterial>(id);
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
  case ComponentType::IndirectLighting:
    return self.template GetComponent<IndirectLighting>(id);
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
  static_assert(sizeof...(T) <= 1 || !(std::is_same_v<typename QueryTraits<T>::Type, Script> || ...), "ForEach: `Script` cannot be combined with other component types in one query");
  // an all-optional query would match every archetype, which is a whole-scene sweep by accident
  static_assert(sizeof...(T) == 0 || !(QueryTraits<T>::optional && ...), "ForEach: a query needs at least one required component to select archetypes with");
  ++self.forEachDepth;
  if constexpr (sizeof...(T) == 0) {
    for (const auto &[id, location] : self.idToLocation)
      func(id);
  } else if constexpr (sizeof...(T) == 1 && std::is_same_v<std::tuple_element_t<0, std::tuple<T...>>, Script>) {
    self.scriptStore.ForEach([&](const EntityID id, auto *script) {
      func(id, script);
    });
  } else {
    // only the required types narrow the archetype set; an optional one is read where present
    ComponentMask mask;
    (SetRequiredBit<T>(mask), ...);
    self.archetypeRegistry.ForEachArchetype([&](auto &archetype) {
      if ((mask & archetype.signature) != mask)
        return;
      // null for an optional type this archetype lacks, and hoisted out of the row loop either way
      auto columns = std::make_tuple(archetype.template GetColumn<typename QueryTraits<T>::Type>()...);
      const auto rowCount = archetype.entities.size();
      for (size_t row = 0; row < rowCount; ++row) {
        const auto id = archetype.entities[row];
        std::apply([&](auto *...col) { func(id, (col ? &col->components[row] : nullptr)...); }, columns);
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
