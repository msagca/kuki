#pragma once
#include <archetype.hpp>
#include <component_type.hpp>
#include <concepts.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <memory>
#include <unordered_map>
namespace kuki {
struct EntityLocation {
  ComponentMask signature;
  size_t row{};
};
struct MoveResult {
  EntityLocation location;
  EntityID displacedEntity{};
  size_t displacedRow{};
};
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
  auto Clear() -> void;
  auto GetArchetype(this auto &, const ComponentMask &) -> decltype(auto);
  auto GetOrCreateArchetype(const ComponentMask &) -> Archetype *;
  auto ForEachArchetype(this auto &, auto &&) -> void;
  auto MoveEntity(const EntityID, const EntityLocation &from, const ComponentMask &newMask) -> MoveResult;
  auto RemoveEntity(const EntityID, const EntityLocation &from) -> EntityID;
private:
  std::unordered_map<ComponentMask, std::unique_ptr<Archetype>> archetypes;
  static auto CreateColumn(const ComponentType) -> std::unique_ptr<IArchetypeColumn>;
};
auto ArchetypeRegistry::GetArchetype(this auto &self, const ComponentMask &mask) -> decltype(auto) {
  auto it = self.archetypes.find(mask);
  if (it == self.archetypes.end())
    return ConstCorrectPointer<decltype(self), Archetype>(nullptr);
  return ConstCorrectPointer<decltype(self), Archetype>(it->second.get());
}
auto ArchetypeRegistry::ForEachArchetype(this auto &self, auto &&func) -> void {
  for (auto &[mask, archetype] : self.archetypes)
    func(*archetype);
}
} // namespace kuki
