#pragma once
#include <archetype.hpp>
#include <archetype_registry.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <component_traits.hpp>
#include <component_type.hpp>
#include <concepts.hpp>
#include <cstdint>
#include <functional>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <string>
#include <transform.hpp>
#include <tuple>
#include <type_list.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace kuki {
/// @brief Marks a component a query reads where it exists, rather than one it filters on.
///
/// `ForEach<Transform, Optional<BoundingBox>>` visits every entity carrying a transform, and hands
/// the callback a bounds pointer that is null for those without one.
///
/// The point is where the component is read from. A required type narrows the archetypes a query
/// visits, so asking for one an entity might not have would silently skip that entity; the usual
/// way around that is to fetch it by id inside the loop, which costs a hash lookup per entity.
/// Naming it optional instead keeps the archetype set wide and still reads the component straight
/// out of its column, which is the whole reason the storage is laid out in columns.
template <typename T>
struct Optional {};
/// @brief Marks a component a query skips the archetypes of. The callback is handed nothing for it.
///
/// `ForEach<Transform, Without<Camera>>` is every transform that is not a camera's.
template <typename T>
struct Without {};
/// @brief Splits a query parameter into the component it names and what the query does with it.
template <typename T>
struct QueryTraits {
  using Type = T;
  static constexpr auto optional = false;
  static constexpr auto excluded = false;
};
template <typename T>
struct QueryTraits<Optional<T>> {
  using Type = T;
  static constexpr auto optional = true;
  static constexpr auto excluded = false;
};
template <typename T>
struct QueryTraits<Without<T>> {
  using Type = T;
  static constexpr auto optional = false;
  static constexpr auto excluded = true;
};
template <typename Q>
inline constexpr uint64_t QueryBit = uint64_t{1} << ComponentIdOf<typename QueryTraits<Q>::Type>;
/// @brief The components an archetype must have for a query to visit it.
template <typename... Qs>
inline constexpr uint64_t RequiredBits = (((QueryTraits<Qs>::optional || QueryTraits<Qs>::excluded) ? uint64_t{0} : QueryBit<Qs>) | ... | uint64_t{0});
/// @brief The components an archetype must not have for a query to visit it.
template <typename... Qs>
inline constexpr uint64_t ExcludedBits = ((QueryTraits<Qs>::excluded ? QueryBit<Qs> : uint64_t{0}) | ... | uint64_t{0});
/// @brief Whether an archetype with these component bits is one a query visits.
template <typename... Qs>
constexpr auto QueryMatches(const uint64_t bits) -> bool {
  return (bits & RequiredBits<Qs...>) == RequiredBits<Qs...> && (bits & ExcludedBits<Qs...>) == 0;
}
template <typename... Qs>
concept IsQuery = (IsComponent<typename QueryTraits<Qs>::Type> && ...) && RequiredBits<Qs...> != 0 && (RequiredBits<Qs...> & ExcludedBits<Qs...>) == 0;
/// @brief The entities of one scene, or of one asset manager's prefabs: their names and hierarchy,
/// and their components, stored by archetype.
///
/// Holds entities of the archetypes it was told about with `Declare`, and no others. An entity is
/// made of one (`Create<A>`) and keeps it: there is no adding a component to an entity and no
/// taking one away. See `Archetype` for why.
class KUKI_ENGINE_API EntityManager {
public:
  /// @brief Tells this manager about an archetype, so that entities can be made of it.
  ///
  /// Declaring one twice does nothing. A scene is given every archetype of its application's
  /// `World` when it is made (`SceneManager::Create`), so this is seldom called by hand.
  auto Declare(const ArchetypeSpec &) -> void;
  template <IsArchetype A>
  auto Declare() -> void;
  auto IsDeclared(const ComponentMask &) const -> bool;
  /// @brief Parents one entity to another, optionally preserving the child's world transform.
  ///
  /// Both must have a `Transform`: the hierarchy is the transforms' hierarchy, and an entity with
  /// none has nothing to inherit. It used to add one to whichever lacked it, which changed the
  /// entity's archetype as a side effect of parenting it.
  ///
  /// Only marks the update order stale rather than rebuilding it. Instantiating a model calls this
  /// once per node, and a rebuild walks the whole scene, so rebuilding eagerly made importing cost
  /// time quadratic in the node count — a few thousand nodes took seconds, spent almost entirely
  /// on rewriting an order that the next call would rewrite again.
  ///
  /// @return True when the link was established.
  auto AddChild(const EntityID, const EntityID, bool = false) -> bool;
  /// @brief Deletes every entity, and keeps every archetype that was declared.
  auto Clear() -> void;
  /// @brief Makes a copy of an entity of another manager, and of its children, here.
  ///
  /// Of the same archetype, which this manager must have been told about. Invalid if not.
  auto CopyFrom(const EntityManager &, const EntityID) -> EntityID;
  auto CopyTo(const EntityID, EntityManager &) const -> EntityID;
  /// @brief Copies the components two entities share, from one of another manager to one here.
  ///
  /// @return The components the source had and the destination did not, which were left behind.
  auto CopyComponents(const EntityManager &, const EntityID from, const EntityID to) -> ComponentMask;
  /// @brief Makes an entity of an archetype, its components at their defaults.
  ///
  /// Allowed while a query is running, unlike `Delete`, which waits for it to finish. The new
  /// entity is not visited by the query that made it. But a component the query handed its callback
  /// can move when a row is added to the same archetype, so a callback that makes an entity of the
  /// archetype it is visiting must not use what it was handed for the entity it is on afterwards.
  ///
  /// Invalid, with an error in the log, if the archetype was never declared here.
  template <IsArchetype A>
  auto Create(std::string = "") -> EntityID;
  /// @brief Makes an entity of the archetype with exactly these components, if one was declared.
  ///
  /// For what learns the components at run time: a scene file, a copy from another manager.
  auto Create(const ComponentMask &, std::string = "") -> EntityID;
  /// @brief Makes an entity of a drawn archetype in one of its pools, making the pool if it is new.
  ///
  /// A key with no mesh makes it in the archetype's own table, where it is not drawn.
  template <IsArchetype A>
  auto Create(const PoolKey &, std::string = "") -> EntityID;
  auto Create(const ComponentMask &, const PoolKey &, std::string = "") -> EntityID;
  /// @brief Makes a pool of a drawn archetype ahead of its entities, with settings of its own.
  ///
  /// A pool made by an entity's `Create` takes the default settings, which are empty bounds here;
  /// `Application` fills them in from the mesh. False if the archetype was never declared, is not
  /// drawn, or the key has no mesh. A pool that is already there keeps the settings it was made with.
  auto CreatePool(const ComponentMask &, const PoolKey &, const PoolSettings & = {}) -> bool;
  /// @brief Moves an entity of a drawn archetype to another pool of it, keeping its id and its
  /// components: how it is given another mesh or material, which are its pool's.
  ///
  /// A structural change, as making or deleting one is, and like `Delete` it waits for a running
  /// query to end. A key with no mesh moves it to the archetype's own table, where it is not drawn.
  auto MoveToPool(const EntityID, const PoolKey &) -> bool;
  /// @brief The pool an entity is drawn from, or an empty key if it is not in one.
  auto GetPool(const EntityID) const -> PoolKey;
  /// @brief Whether a pool of these components was made here.
  auto HasPool(const ComponentMask &, const PoolKey &) const -> bool;
  /// @brief An entity's bounds around its origin: its own `BoundingBox` if it has one, otherwise
  /// its pool's. Empty for one with neither.
  auto GetBounds(const EntityID) const -> BoundingBox;
  /// @brief Calls `func(table)` for every pool that has entities in it.
  auto ForEachPool(this auto &, auto &&) -> void;
  auto Delete(const EntityID) -> bool;
  auto ForEachChild(this const auto &, const EntityID, auto &&) -> void;
  auto ForEachComponent(this const auto &, const EntityID, auto &&) -> void;
  auto ForEachRoot(this const auto &, auto &&) -> void;
  auto GetComponent(this auto &self, const EntityID, const ComponentType) -> ConstBasedValue<decltype(self), ConstComponentVariant, ComponentVariant>;
  auto GetComponentTypes(const EntityID) const -> std::vector<ComponentType>;
  auto GetCount() const -> size_t;
  auto GetID(const std::string &) const -> EntityID;
  auto GetStructuralGeneration() const -> size_t;
  auto GetName(const EntityID) const -> std::string;
  auto GetParent(const EntityID) const -> EntityID;
  /// @brief The components of an entity's archetype. Empty for an id that is not an entity here.
  auto GetSignature(const EntityID) const -> ComponentMask;
  auto HasChildren(const EntityID) const -> bool;
  auto IsEntity(const EntityID) const -> bool;
  auto IsEntity(const std::string &) const -> bool;
  /// @brief Whether any entity at all carries this component. What enforces `IsSceneSingleton`.
  ///
  /// Reads the archetype signatures rather than fetching the component, because the question is
  /// whether one exists and not what is in it, and every entity's signature is already to hand.
  auto HasComponentAnywhere(const ComponentType) const -> bool;
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
  /// next update rebuild and mark everything anyway. Making or deleting an entity does that, so a
  /// new entity's transform needs no call here.
  auto MarkTransformDirty(const EntityID) -> void;
  auto Rename(const EntityID, std::string) -> bool;
  /// @brief Calls `func(id, components...)` for every entity of every archetype the query matches.
  ///
  /// A parameter is a component the archetype must have, `Optional<T>` for one read where it exists
  /// (null where it does not), or `Without<T>` for one it must not have, which hands the callback
  /// nothing. A query that names something that is not a component, requires nothing, or requires
  /// and excludes the same component does not compile. `World::ForEach` goes further and refuses a
  /// query that no archetype of the game could ever match.
  template <typename... Q>
  auto ForEach(this auto &, auto &&) -> void;
  /// @brief `ForEach` over the entities of one archetype, which the compiler has found the query
  /// matches.
  ///
  /// What `World::ForEach` runs once for each archetype of its game that the query matches, so the
  /// archetypes it would skip are never looked at, and a component the query reads where it exists
  /// is known to be there or not before the loop starts rather than tested for on every row.
  template <IsArchetype A, typename... Q>
  auto ForEachOf(this auto &, auto &&) -> void;
  template <IsComponent T>
  auto GetAny(this auto &self) -> ConstCorrectPointer<decltype(self), T>;
  template <IsComponent... T>
  auto GetComponent(this auto &, const EntityID) -> decltype(auto);
  template <IsComponent... T>
  auto GetComponent(this auto &, const std::string &) -> decltype(auto);
  template <IsComponent... T>
  auto HasComponent(const EntityID) const -> bool;
  template <typename... T>
  auto UpdateComponents() -> void;
private:
  EntityID nextId{0};
  ArchetypeRegistry archetypeRegistry;
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
  /// Pointers into archetype columns, so anything that adds a row to a table or takes one out of it
  /// can invalidate them -- a column may move when it grows, and swapping the last row into a gap
  /// moves that one; `transformCacheGeneration` is what notices.
  std::vector<Transform *> transformCache;
  /// @brief Each entry's world matrix, beside `transformCache` and invalidated with it. A column of
  /// its own in the tables (see `Transform`), so a pointer of its own here.
  std::vector<WorldMatrix *> worldCache;
  /// @brief The table each entry's row is in, so that moving an entity raises its pool's version.
  std::vector<uint32_t> transformTable;
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
  void DeleteRecords(const EntityID);
  auto CreateIn(const uint32_t table, std::string name) -> EntityID;
  auto DrainPendingStructuralChanges() const -> void;
  auto RebuildTransformOrder() -> void;
  auto RebuildTransformCache() -> void;
  auto IsTransformCacheStale() const -> bool;
  void AppendTransformOrder(const EntityID);
  auto UpdateTransforms() -> void;
  /// @brief Where a query's component comes from in one table: a column, or nothing for `Without`.
  template <typename Q, typename Table>
  static auto QueryColumn(Table &table) {
    if constexpr (QueryTraits<Q>::excluded)
      return std::tuple<>{};
    else
      return std::tuple(table.template GetColumn<typename QueryTraits<Q>::Type>());
  }
  /// @brief An optional component an archetype is known not to have, as the null it hands over.
  template <typename Pointer>
  struct NoColumn {};
  /// @brief `QueryColumn` for an archetype known at compile time.
  template <IsArchetype A, typename Q, typename Table>
  static auto KnownColumn(Table &table) {
    using C = typename QueryTraits<Q>::Type;
    if constexpr (QueryTraits<Q>::excluded)
      return std::tuple<>{};
    else if constexpr (A::template Has<C>)
      return std::tuple(table.template GetColumn<C>());
    else
      return std::tuple(NoColumn<ConstCorrectPointer<Table, C>>{});
  }
  template <typename Column>
  static auto RowOf(Column *column, const size_t row) {
    return &column->components[row];
  }
  template <typename Pointer>
  static auto RowOf(NoColumn<Pointer>, const size_t) -> Pointer {
    return nullptr;
  }
};
template <IsArchetype A>
auto EntityManager::Declare() -> void {
  // Made once per archetype type and kept: the columns in it are only templates to copy.
  static const auto spec = MakeArchetypeSpec<A>();
  Declare(spec);
}
template <IsArchetype A>
auto EntityManager::Create(std::string name) -> EntityID {
  return Create(A::Mask(), std::move(name));
}
template <IsArchetype A>
auto EntityManager::Create(const PoolKey &pool, std::string name) -> EntityID {
  static_assert(A::template Has<Pooled>, "Create: only an archetype that is drawn -- one with `Renderable` in it -- has pools");
  return Create(A::Mask(), pool, std::move(name));
}
auto EntityManager::ForEachPool(this auto &self, auto &&func) -> void {
  self.archetypeRegistry.ForEachTable([&](auto &table) {
    if (table.pool && !table.entities.empty())
      func(table);
  });
}
auto EntityManager::ForEachChild(this const auto &self, const EntityID parent, auto &&func) -> void {
  if (auto it = self.idToChildren.find(parent); it != self.idToChildren.end()) {
    const auto children = it->second;
    for (const auto &childId : children)
      func(childId);
  }
}
auto EntityManager::ForEachComponent(this const auto &self, const EntityID id, auto &&func) -> void {
  Component::ForEachSetType(self.GetSignature(id), [&](const ComponentType type) {
    auto component = self.GetComponent(id, type);
    func(component);
  });
}
auto EntityManager::ForEachRoot(this const auto &self, auto &&func) -> void {
  const auto roots = self.rootEntities;
  for (const auto &id : roots)
    func(id);
}
auto EntityManager::GetComponent(this auto &self, const EntityID id, const ComponentType type) -> ConstBasedValue<decltype(self), ConstComponentVariant, ComponentVariant> {
  // Generated from the list of components rather than written out as a switch. The switch had a
  // `default` that handed back the entity's transform, so a component someone forgot to add to it
  // came back as the wrong component and not as nothing.
  using Result = ConstBasedValue<decltype(self), ConstComponentVariant, ComponentVariant>;
  Result result{};
  ForEachType(EngineComponents{}, [&]<typename C>() {
    if (ComponentIdOf<C> == static_cast<ComponentId>(type))
      result = Result(self.template GetComponent<C>(id));
  });
  return result;
}
template <typename... Q>
auto EntityManager::ForEach(this auto &self, auto &&func) -> void {
  ++self.forEachDepth;
  if constexpr (sizeof...(Q) == 0) {
    for (const auto &[id, location] : self.idToLocation)
      func(id);
  } else {
    static_assert(IsQuery<Q...>, "ForEach: every parameter must be a component, Optional<component> or Without<component>; at least one must be required; and none may be both required and excluded");
    const ComponentMask required(RequiredBits<Q...>);
    const ComponentMask excluded(ExcludedBits<Q...>);
    self.archetypeRegistry.ForEachTable([&](auto &table) {
      if ((table.signature & required) != required || (table.signature & excluded).any())
        return;
      // null for an optional type this archetype lacks, and hoisted out of the row loop either way
      auto columns = std::tuple_cat(QueryColumn<Q>(table)...);
      const auto rowCount = table.entities.size();
      for (size_t row = 0; row < rowCount; ++row) {
        const auto id = table.entities[row];
        std::apply([&](auto *...column) { func(id, (column ? &column->components[row] : nullptr)...); }, columns);
      }
    });
  }
  --self.forEachDepth;
  if (self.forEachDepth == 0)
    self.DrainPendingStructuralChanges();
}
template <IsArchetype A, typename... Q>
auto EntityManager::ForEachOf(this auto &self, auto &&func) -> void {
  static_assert(IsQuery<Q...>, "ForEachOf: every parameter must be a component, Optional<component> or Without<component>; at least one must be required; and none may be both required and excluded");
  static_assert(QueryMatches<Q...>(A::bits), "ForEachOf: the archetype is not one the query visits");
  ++self.forEachDepth;
  // The archetype's own table and every pool of it: a drawn archetype's entities are spread over a
  // table per mesh and material, and a query over the archetype is over all of them.
  self.archetypeRegistry.ForEachTableOf(A::Mask(), [&](auto &table) {
    auto columns = std::tuple_cat(KnownColumn<A, Q>(table)...);
    const auto rowCount = table.entities.size();
    for (size_t row = 0; row < rowCount; ++row) {
      const auto id = table.entities[row];
      std::apply([&](auto... column) { func(id, RowOf(column, row)...); }, columns);
    }
  });
  --self.forEachDepth;
  if (self.forEachDepth == 0)
    self.DrainPendingStructuralChanges();
}
template <IsComponent T>
auto EntityManager::GetAny(this auto &self) -> ConstCorrectPointer<decltype(self), T> {
  ConstCorrectPointer<decltype(self), T> result = nullptr;
  self.archetypeRegistry.ForEachTable([&](auto &table) {
    if (result)
      return;
    if (auto *column = table.template GetColumn<T>(); column && column->Size() > 0)
      result = &column->components[0];
  });
  return result;
}
template <IsComponent... T>
auto EntityManager::GetComponent(this auto &self, const EntityID id) -> decltype(auto) {
  if constexpr (sizeof...(T) == 1) {
    using C = std::tuple_element_t<0, std::tuple<T...>>;
    using PointerType = ConstCorrectPointer<decltype(self), C>;
    auto it = self.idToLocation.find(id);
    if (it == self.idToLocation.end())
      return PointerType(nullptr);
    auto *column = self.archetypeRegistry.GetTable(it->second.table)->template GetColumn<C>();
    if (!column)
      return PointerType(nullptr);
    return PointerType(&column->components[it->second.row]);
  } else
    return std::make_tuple(self.template GetComponent<T>(id)...);
}
template <IsComponent... T>
auto EntityManager::GetComponent(this auto &self, const std::string &name) -> decltype(auto) {
  EntityID id{};
  auto ids = self.nameToId.equal_range(name);
  if (auto it = ids.first; it != ids.second)
    id = it->second;
  return self.template GetComponent<T...>(id);
}
template <IsComponent... T>
auto EntityManager::HasComponent(const EntityID id) const -> bool {
  const auto mask = MaskOf<T...>();
  return (GetSignature(id) & mask) == mask;
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
