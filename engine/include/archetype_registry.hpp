#pragma once
#include <archetype.hpp>
#include <component_traits.hpp>
#include <concepts.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
namespace kuki {
struct EntityLocation {
  /// The archetype's place in the registry, which never changes once it is declared.
  uint32_t table{};
  size_t row{};
};
/// @brief The archetypes an entity manager was told about, and their tables.
///
/// Declared, not discovered. An archetype exists because something said it would be used, before
/// any entity of it was made, and the set does not change after: nothing here makes an archetype
/// because an entity happened to need one. Tables are kept in the order they were declared, which
/// is the order every query visits them in.
class KUKI_ENGINE_API ArchetypeRegistry {
public:
  // Declaring any constructor suppresses the implicit default one, so it is asked for back.
  ArchetypeRegistry() = default;
  /// @brief Not copyable: it owns what it holds through `unique_ptr`.
  ///
  /// Explicit rather than left implicit because exporting a class from a shared library
  /// instantiates its implicit members too, and the implicit copy of a container of `unique_ptr`
  /// does not compile. Nothing copies this, so the declaration costs nothing and says so.
  ArchetypeRegistry(const ArchetypeRegistry &) = delete;
  auto operator=(const ArchetypeRegistry &) -> ArchetypeRegistry & = delete;
  /// @brief Adds an archetype. Declaring one that is already here does nothing.
  auto Declare(const ArchetypeSpec &) -> void;
  /// @brief The archetype's own table: the one that is not a pool.
  auto Find(const ComponentMask &) const -> std::optional<uint32_t>;
  /// @brief The table of one pool of an archetype, if it has been made.
  auto Find(const ComponentMask &, const PoolKey &) const -> std::optional<uint32_t>;
  /// @brief The table of one pool of a declared archetype, made empty if it was not there.
  ///
  /// The only way the set of tables grows after the archetypes are declared: a pool is a table, and
  /// which meshes and materials a game draws with is not known to the compiler. The settings are
  /// taken only when the pool is made. Nothing for an archetype that was not declared, or a key with
  /// no mesh, which names the archetype's own table rather than a pool.
  auto FindOrCreate(const ComponentMask &, const PoolKey &, const PoolSettings & = {}) -> std::optional<uint32_t>;
  auto GetTable(this auto &, const uint32_t) -> decltype(auto);
  auto GetTableCount() const -> size_t;
  auto ForEachTable(this auto &, auto &&) -> void;
  /// @brief Calls `func(table)` for each table of one archetype: its own, then its pools'.
  auto ForEachTableOf(this auto &, const ComponentMask &, auto &&) -> void;
  /// @brief A new row of default components at the end of a table.
  auto Append(const uint32_t, const EntityID) -> size_t;
  /// @brief Takes a row out by moving the last one into it, and says whose row that was.
  auto Remove(const EntityLocation &) -> EntityID;
  /// @brief Empties every table and keeps every declaration.
  auto ClearRows() -> void;
private:
  std::vector<std::unique_ptr<ArchetypeTable>> tables;
  std::unordered_map<ComponentMask, uint32_t> tableByMask;
  /// Every table of each archetype, its own first and its pools in the order they were made.
  std::unordered_map<ComponentMask, std::vector<uint32_t>> tablesOfMask;
  struct PoolOf {
    ComponentMask mask;
    PoolKey pool;
    auto operator==(const PoolOf &) const -> bool = default;
  };
  struct PoolOfHash {
    auto operator()(const PoolOf &key) const noexcept -> size_t {
      auto seed = std::hash<ComponentMask>{}(key.mask);
      hash_combine(seed, std::hash<PoolKey>{}(key.pool));
      return seed;
    }
  };
  std::unordered_map<PoolOf, uint32_t, PoolOfHash> tableByPool;
};
auto ArchetypeRegistry::GetTable(this auto &self, const uint32_t index) -> decltype(auto) {
  return static_cast<ConstCorrectPointer<decltype(self), ArchetypeTable>>(self.tables[index].get());
}
auto ArchetypeRegistry::ForEachTableOf(this auto &self, const ComponentMask &mask, auto &&func) -> void {
  if (auto it = self.tablesOfMask.find(mask); it != self.tablesOfMask.end())
    for (const auto index : it->second)
      func(*static_cast<ConstCorrectPointer<decltype(self), ArchetypeTable>>(self.tables[index].get()));
}
auto ArchetypeRegistry::ForEachTable(this auto &self, auto &&func) -> void {
  for (auto &table : self.tables)
    func(*static_cast<ConstCorrectPointer<decltype(self), ArchetypeTable>>(table.get()));
}
} // namespace kuki
