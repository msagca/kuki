#pragma once
#include <application.hpp>
#include <archetype.hpp>
#include <component_traits.hpp>
#include <engine_archetypes.hpp>
#include <entity_manager.hpp>
#include <game_builder.hpp>
#include <string>
#include <type_list.hpp>
#include <utility>
#include <vector>
namespace kuki {
namespace detail {
  template <typename List>
  inline constexpr bool DistinctSignatures = true;
  template <typename A, typename... As>
  inline constexpr bool DistinctSignatures<TypeList<A, As...>> = ((A::bits != As::bits) && ...) && DistinctSignatures<TypeList<As...>>;
  template <typename List>
  struct ComponentsOfImpl;
  template <typename... As>
  struct ComponentsOfImpl<TypeList<As...>> {
    using Type = Unique<Concat<typename As::List...>>;
  };
} // namespace detail
/// @brief Every archetype a game makes entities of: the engine's own, and the ones listed.
///
/// A game names one, gives it to its `SystemApplication`, and from then on the compiler knows what
/// the game's entities can be made of. The checks it makes are the point:
///
/// - no two archetypes of the world have the same components, so a signature always names one;
/// - no two components share an id, which is what two `KUKI_COMPONENT` lines given the same index
///   would otherwise do quietly, each overwriting the other's column;
/// - `Create<A>` and the builder from `Game` refuse an archetype the world does not list, which at
///   run time would have made no entity;
/// - `ForEach<Q...>` refuses a query no archetype of the world matches, which at run time would
///   have visited nothing, and visits only the archetypes the compiler found to match.
///
/// Each scene of the application is given the world's archetypes when it is made
/// (`SceneManager::Create`), and none after: a scene holds the archetypes its world lists and no
/// others.
template <IsArchetype... As>
struct World {
  using Archetypes = Concat<EngineArchetypes, TypeList<As...>>;
  using Components = typename detail::ComponentsOfImpl<Archetypes>::Type;
  static_assert(IsUnique<Archetypes>, "World: an archetype is listed twice, or is one of the engine's (EngineArchetypes), which every world has already");
  static_assert(detail::DistinctSignatures<Archetypes>, "World: two archetypes have the same components; an entity's components have to name one archetype");
  static_assert(HasDistinctIds<Components>, "World: two components have the same id -- two KUKI_COMPONENT lines were given the same index");
  /// @brief Whether an archetype with these components is one of the world's.
  template <typename A>
  static constexpr bool Has = AnyType(Archetypes{}, []<typename B>() { return B::bits == A::bits; });
  /// @brief Whether any archetype of the world is one a query visits.
  template <typename... Q>
  static constexpr bool Matches = AnyType(Archetypes{}, []<typename B>() { return QueryMatches<Q...>(B::bits); });
  /// @brief Whether any archetype of the world has the component.
  template <typename C>
  static constexpr bool Uses = Contains<Components, C>;
  /// @brief The archetypes to give each scene, for `SystemApplication`.
  static auto Specs() -> std::vector<ArchetypeSpec> {
    return MakeArchetypeSpecs(Archetypes{});
  }
  /// @brief Makes an entity of one of the world's archetypes in the active scene.
  template <IsArchetype A>
  static auto Create(Application &app, std::string name = "") -> EntityID {
    static_assert(Has<A>, "World::Create: this world does not declare that archetype, so no scene of it can hold one");
    return app.CreateEntity<A>(std::move(name));
  }
  /// @brief Makes an entity of one of the world's drawn archetypes in the pool of a mesh and a
  /// material. See `Application::CreateEntity`.
  template <IsArchetype A>
  static auto Create(Application &app, const PoolKey &pool, std::string name = "") -> EntityID {
    static_assert(Has<A>, "World::Create: this world does not declare that archetype, so no scene of it can hold one");
    return app.CreateEntity<A>(pool, std::move(name));
  }
  /// @brief Makes a pool of one of the world's drawn archetypes ahead of its entities.
  template <IsArchetype A>
  static auto CreatePool(Application &app, const PoolKey &pool, PoolSettings settings = {}) -> bool {
    static_assert(Has<A>, "World::CreatePool: this world does not declare that archetype");
    return app.CreatePool<A>(pool, std::move(settings));
  }
  /// @brief Places a model with a root of one of the world's archetypes. See `AssetManager::Instantiate`.
  template <IsArchetype Root = ModelRoot>
  static auto Instantiate(Application &app, const std::string &asset) -> EntityID {
    static_assert(Has<Root>, "World::Instantiate: this world does not declare that archetype for a model's root");
    return app.InstantiateAsset<Root>(asset);
  }
  /// @brief Calls `func(id, components...)` for every entity of the active scene the query matches.
  ///
  /// As `EntityManager::ForEach`, but the archetypes it visits are chosen by the compiler: the loop
  /// runs once for each archetype of the world that matches, and the rest are never looked at. A
  /// query that matches none of them is not one that would have found nothing; it is a mistake, and
  /// does not compile.
  template <typename... Q>
  static auto ForEach(Application &app, auto &&func) -> void {
    static_assert(IsQuery<Q...>, "World::ForEach: every parameter must be a component, Optional<component> or Without<component>; at least one must be required; and none may be both required and excluded");
    static_assert(Matches<Q...>, "World::ForEach: no archetype of this world has every component this query requires and none it excludes, so it could never visit anything");
    ForEachType(Archetypes{}, [&]<typename A>() {
      if constexpr (QueryMatches<Q...>(A::bits))
        app.ForEachEntityOf<A, Q...>(func);
    });
  }
  /// @brief One component of an entity of the active scene, or null if the entity has none.
  template <IsComponent C>
  static auto Get(Application &app, const EntityID id) -> C * {
    static_assert(Uses<C>, "World::Get: no archetype of this world has that component");
    return app.GetEntityComponent<C>(id);
  }
  /// @brief The builder, with every entity it makes checked against this world.
  static auto Game(Application &app, const std::string &name = "") -> GameBuilder<World> {
    return GameBuilder<World>(app, name);
  }
};
template <typename T>
inline constexpr bool IsWorldType = false;
template <typename... As>
inline constexpr bool IsWorldType<World<As...>> = true;
template <typename T>
concept IsWorld = IsWorldType<T>;
/// @brief The world of an application that adds no archetypes of its own.
using EngineWorld = World<>;
} // namespace kuki
