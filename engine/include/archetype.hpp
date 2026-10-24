#pragma once
#include <array>
#include <component_traits.hpp>
#include <concepts.hpp>
#include <cstdint>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <memory>
#include <string_view>
#include <type_list.hpp>
#include <vector>
namespace kuki {
/// @brief A named group of components, for writing archetypes in parts.
///
/// `Archetype<Transform, Renderable, Fighter>` rather than spelling out the four components a mesh
/// needs to be drawn. A bundle in an archetype is the same as its components written there, and a
/// bundle may hold bundles.
template <typename... Ts>
struct Components {};
namespace detail {
  template <typename T>
  struct FlattenOne {
    using Type = TypeList<T>;
  };
  template <typename... Ts>
  struct FlattenOne<Components<Ts...>> {
    using Type = Concat<typename FlattenOne<Ts>::Type...>;
  };
  template <typename T>
  struct IsComponentOf {
    static constexpr bool value = IsComponent<T>;
  };
} // namespace detail
/// @brief The components of an entity, fixed when the entity is made.
///
/// An entity is made of one of these and keeps it for as long as it lives: nothing adds a
/// component to an entity or takes one away. What used to be done that way -- a renderer
/// attaching its state the first time it drew something, a model gaining a skeleton once it was
/// placed, a builder adding a transform and then a mesh -- is declared up front, so that the set of
/// archetypes a game can have is known to the compiler. That is what lets a query over components
/// no archetype has be a compile error (`World::ForEach`), and what keeps a pointer into a column
/// valid for as long as its entity is.
///
/// Order does not matter: two archetypes of the same components are the same archetype, and a
/// `World` that declares both is told so.
template <typename... Ts>
struct Archetype {
  using List = Concat<typename detail::FlattenOne<Ts>::Type...>;
  static_assert(sizeof...(Ts) > 0, "an archetype needs at least one component");
  static_assert(Filter<List, detail::IsComponentOf>::size == List::size, "an archetype names a type that is not a component -- declare it with KUKI_COMPONENT");
  static_assert(IsUnique<List>, "an archetype names a component twice, perhaps once on its own and once in a bundle");
  static constexpr uint64_t bits = ListMaskBits<List>;
  template <typename C>
  static constexpr bool Has = Contains<List, C>;
  static auto Mask() -> ComponentMask {
    return ComponentMask(bits);
  }
};
template <typename T>
inline constexpr bool IsArchetypeType = false;
template <typename... Ts>
inline constexpr bool IsArchetypeType<Archetype<Ts...>> = true;
template <typename T>
concept IsArchetype = IsArchetypeType<T>;
/// @brief One component type's values in one archetype, a row per entity.
///
/// Type-erased so that an archetype can hold a game's components, which the engine was compiled
/// without. The column is made where the archetype's type is known -- `MakeArchetypeSpec`, in
/// whatever binary declared it -- and everything else reaches its values through the typed
/// `ArchetypeColumn<T>` with an id that the compiler checked.
struct KUKI_ENGINE_API IArchetypeColumn {
  virtual ~IArchetypeColumn() = default;
  virtual auto Size() const -> size_t = 0;
  virtual auto EmplaceDefault() -> void = 0;
  /// @brief Appends a copy of a row of another column of the same component.
  virtual auto CopyAppend(const IArchetypeColumn &, const size_t) -> void = 0;
  /// @brief Overwrites a row with a copy of a row of another column of the same component.
  virtual auto CopyRow(const IArchetypeColumn &, const size_t from, const size_t to) -> void = 0;
  virtual auto SwapPop(const size_t) -> void = 0;
  virtual auto Clear() -> void = 0;
  /// @brief An empty column of the same component, for declaring the same archetype somewhere else.
  virtual auto CloneEmpty() const -> std::unique_ptr<IArchetypeColumn> = 0;
};
template <IsComponent T>
struct ArchetypeColumn final : public IArchetypeColumn {
  std::vector<T> components;
  auto Size() const -> size_t override {
    return components.size();
  }
  auto EmplaceDefault() -> void override {
    components.emplace_back();
  }
  auto CopyAppend(const IArchetypeColumn &other, const size_t row) -> void override {
    components.push_back(static_cast<const ArchetypeColumn &>(other).components[row]);
  }
  auto CopyRow(const IArchetypeColumn &other, const size_t from, const size_t to) -> void override {
    components[to] = static_cast<const ArchetypeColumn &>(other).components[from];
  }
  auto SwapPop(const size_t row) -> void override {
    const auto last = components.size() - 1;
    if (row != last)
      components[row] = std::move(components[last]);
    components.pop_back();
  }
  auto Clear() -> void override {
    components.clear();
  }
  auto CloneEmpty() const -> std::unique_ptr<IArchetypeColumn> override {
    return std::make_unique<ArchetypeColumn>();
  }
};
/// @brief An archetype as something to hand to an `EntityManager` at run time: its signature, and
/// a column of each of its components.
///
/// The columns are empty; an entity manager that is told about the archetype takes copies of them
/// (`CloneEmpty`) for its own table. Made once per archetype type by `MakeArchetypeSpec`.
struct ArchetypeSpec {
  ComponentMask mask;
  /// One per component, in order of id.
  std::vector<std::shared_ptr<const IArchetypeColumn>> columns;
  /// The id of each column's component, in the same order.
  std::vector<ComponentId> ids;
  /// The name of each column's component (`ComponentTraits<T>::name`), in the same order. What a
  /// scene file records an entity's archetype by, so that a game's components -- which the engine
  /// has no names for of its own -- are written and read back like the engine's.
  std::vector<std::string_view> names;
};
template <IsArchetype A>
auto MakeArchetypeSpec() -> ArchetypeSpec {
  ArchetypeSpec spec;
  spec.mask = A::Mask();
  std::array<std::shared_ptr<const IArchetypeColumn>, MAX_COMPONENTS> byId{};
  std::array<std::string_view, MAX_COMPONENTS> nameById{};
  ForEachType(typename A::List{}, [&]<typename C>() {
    byId[ComponentIdOf<C>] = std::make_shared<ArchetypeColumn<C>>();
    nameById[ComponentIdOf<C>] = ComponentTraits<C>::name;
  });
  for (size_t id = 0; id < MAX_COMPONENTS; ++id)
    if (byId[id]) {
      spec.columns.push_back(std::move(byId[id]));
      spec.ids.push_back(static_cast<ComponentId>(id));
      spec.names.push_back(nameById[id]);
    }
  return spec;
}
template <IsArchetype... As>
auto MakeArchetypeSpecs(TypeList<As...>) -> std::vector<ArchetypeSpec> {
  return {MakeArchetypeSpec<As>()...};
}
/// @brief The entities of one archetype in one entity manager, and their components, by column.
struct ArchetypeTable {
  ArchetypeTable() {
    columnIndexById.fill(-1);
  }
  ComponentMask signature;
  std::vector<EntityID> entities;
  std::array<int, MAX_COMPONENTS> columnIndexById;
  std::vector<std::unique_ptr<IArchetypeColumn>> columns;
  /// @brief The column of one component, or null if this archetype does not have it.
  ///
  /// The index is the component's id, known to the compiler, so this costs one array read and no
  /// lookup by type.
  template <IsComponent T>
  auto GetColumn(this auto &self) -> ConstCorrectPointer<decltype(self), ArchetypeColumn<T>> {
    const auto index = self.columnIndexById[ComponentIdOf<T>];
    if (index < 0)
      return nullptr;
    return static_cast<ConstCorrectPointer<decltype(self), ArchetypeColumn<T>>>(self.columns[static_cast<size_t>(index)].get());
  }
  auto GetColumn(this auto &self, const ComponentId id) -> ConstCorrectPointer<decltype(self), IArchetypeColumn> {
    const auto index = self.columnIndexById[id];
    if (index < 0)
      return nullptr;
    return self.columns[static_cast<size_t>(index)].get();
  }
};
} // namespace kuki
