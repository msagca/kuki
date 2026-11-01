#pragma once
#include <cstdint>
#include <id.hpp>
#include <vector>
namespace kuki {
/// @brief Which slot of a `SlotStore` holds an entity's data, kept in a component in place of the data.
///
/// A component is trivially copyable, so what varies in length -- a skeleton's nodes, a pose --
/// cannot be in it. It is kept in a store a scene or a system owns, and the component holds this.
struct SlotHandle {
  static constexpr uint32_t NONE = UINT32_MAX;
  uint32_t index{NONE};
};
/// @brief Data of a length that varies, one slot for each entity that has some.
///
/// A slot remembers the entity it was claimed for and answers to that entity alone. A component
/// copied to another entity -- a prefab placed, an entity duplicated in the editor -- carries its
/// handle with it, finds nothing behind it, and claims a slot of its own, rather than sharing one
/// with the entity it was copied from and each writing over the other's. Entity ids are never given
/// out twice in a scene, so an entity that has gone cannot be mistaken for one that came after it.
///
/// Nothing is told when an entity goes. Whoever owns the store sweeps it on its own schedule,
/// asking of each slot whether its entity is still there (`Sweep`), which is the rule for every
/// kind of clean-up in the engine: state looked at, not events listened for.
template <typename T>
class SlotStore {
public:
  auto Find(const SlotHandle handle, const EntityID owner) -> T * {
    return owner && handle.index < slots.size() && slots[handle.index].owner == owner ? &slots[handle.index].value : nullptr;
  }
  auto Find(const SlotHandle handle, const EntityID owner) const -> const T * {
    return owner && handle.index < slots.size() && slots[handle.index].owner == owner ? &slots[handle.index].value : nullptr;
  }
  /// @brief The entity's slot, after giving it one if the handle does not lead to one of its own.
  auto Claim(SlotHandle &handle, const EntityID owner) -> T & {
    if (auto *found = Find(handle, owner); found)
      return *found;
    if (free.empty()) {
      handle.index = static_cast<uint32_t>(slots.size());
      slots.push_back({owner, T{}});
    } else {
      handle.index = free.back();
      free.pop_back();
      slots[handle.index].owner = owner;
    }
    return slots[handle.index].value;
  }
  /// @brief Empties every slot whose entity `keep(entity)` says is gone, for the next claim to take.
  auto Sweep(auto &&keep) -> void {
    for (uint32_t index = 0; index < slots.size(); ++index) {
      auto &slot = slots[index];
      if (!slot.owner || keep(slot.owner))
        continue;
      slot = {};
      free.push_back(index);
    }
  }
  auto Clear() -> void {
    slots.clear();
    free.clear();
  }
  /// @brief How many slots belong to an entity.
  auto Count() const -> size_t {
    return slots.size() - free.size();
  }
private:
  struct Slot {
    EntityID owner{};
    T value{};
  };
  std::vector<Slot> slots;
  std::vector<uint32_t> free;
};
} // namespace kuki
