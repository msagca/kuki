#pragma once
#include <slot_store.hpp>
namespace kuki {
/// @brief Marks the root of a placed model whose nodes can be moved: the slot of its `Rig` in the scene.
struct Skeleton {
  SlotHandle rig;
};
} // namespace kuki
