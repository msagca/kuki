#pragma once
#include <cstddef>
namespace kuki {
/// @brief Frames between one collection round and the next.
///
/// Collection walks every key a pool holds, so it is not something to do in a frame's budget when
/// nothing has changed. Four seconds at sixty frames is slow enough to disappear into the noise and
/// quick enough that a resolution nobody is using any more is gone before a second one arrives.
inline constexpr size_t POOL_COLLECT_INTERVAL = 240;
/// @brief Rounds a key may go untouched before everything under it is released.
///
/// Not one, because work in this engine arrives in bursts: an asset preview borrows a framebuffer
/// and three buffers and gives them all back within the same frame, and the next preview may be a
/// second later. Collecting on the first quiet round would make every burst pay to allocate what
/// the last burst had just finished with. Four rounds is long enough to ride out the gap between
/// bursts and short enough that a size which really has been abandoned does not outlive the minute.
inline constexpr size_t POOL_IDLE_ROUNDS = 4;
/// @brief What a pool is holding, for a caller that wants to show or log it.
///
/// `inUse` counts what has been requested and not yet released; `available` counts what is sitting
/// in the free lists waiting to be handed out again. The two together are what the pool has actually
/// allocated, and the ratio between them is what says whether collection has anything to do.
struct PoolUsage {
  size_t inUse{};
  size_t available{};
  size_t keys{};
  auto operator+=(const PoolUsage &other) -> PoolUsage & {
    inUse += other.inUse;
    available += other.available;
    keys += other.keys;
    return *this;
  }
};
} // namespace kuki
