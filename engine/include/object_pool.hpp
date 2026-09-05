#pragma once
#include <algorithm>
#include <concepts.hpp>
#include <cstddef>
#include <pool_policy.hpp>
#include <vector>
namespace kuki {
/// @brief Reusable resources of one kind, with a single free list.
///
/// The unkeyed sibling of `KeyedPool`, for resources that carry no description worth grouping by --
/// a framebuffer object is a name and a set of attachment points, and one is as good as another.
/// The accounting and the collection policy are the same, minus the part about abandoned keys:
/// there is only ever one list here, so it is trimmed rather than forgotten.
template <typename V>
class ObjectPool {
public:
  virtual ~ObjectPool() = default;
  auto PreAllocate(const size_t) -> void;
  auto Request() -> V;
  template <std::convertible_to<V>... Vals>
  auto Release(Vals &&...) -> void;
  /// @brief Trims the free list to the busiest moment of the round just ended. One round.
  ///
  /// Only ever touches what is waiting, never what is lent out, so it cannot take a resource away
  /// from its user. What is kept is the peak minus what is still out: enough to reach the busiest
  /// moment the recent past actually had, counting the ones already in someone's hands.
  ///
  /// @return How many resources were released, for the caller to report.
  auto Collect() -> size_t;
  /// @brief Releases everything, lent out or not. For teardown, while the API is still alive.
  auto Clear() -> void;
  auto GetUsage() const -> PoolUsage;
protected:
  std::vector<V> pool;
  /// @brief Lent out and not yet given back, and the highest that has been since the last round.
  size_t inUse{};
  size_t peak{};
  virtual auto Allocate() -> V = 0;
  virtual auto Reallocate(V &) -> void;
  /// @brief Hands one resource back to the graphics API. See `KeyedPool::Deallocate` for why this
  /// is never called from a destructor.
  virtual auto Deallocate(V &) -> void;
};
template <typename V>
auto ObjectPool<V>::PreAllocate(const size_t count) -> void {
  if (count == 0)
    return;
  pool.reserve(pool.size() + count);
  for (size_t i = 0; i < count; ++i)
    pool.emplace_back(Allocate());
}
template <typename V>
auto ObjectPool<V>::Request() -> V {
  ++inUse;
  peak = std::max(peak, inUse);
  if (pool.empty())
    return Allocate();
  auto val = std::move(pool.back());
  pool.pop_back();
  return val;
}
template <typename V>
template <std::convertible_to<V>... Vals>
auto ObjectPool<V>::Release(Vals &&...vals) -> void {
  (pool.push_back(std::forward<Vals>(vals)), ...);
  // Floored rather than wrapped, for the reason argued in `KeyedPool::Release`.
  inUse -= std::min(inUse, sizeof...(Vals));
}
template <typename V>
auto ObjectPool<V>::Collect() -> size_t {
  size_t freed = 0;
  const auto keep = peak > inUse ? peak - inUse : 0;
  while (pool.size() > keep) {
    Deallocate(pool.back());
    pool.pop_back();
    ++freed;
  }
  peak = inUse;
  return freed;
}
template <typename V>
auto ObjectPool<V>::Clear() -> void {
  for (auto &val : pool)
    Deallocate(val);
  pool.clear();
  inUse = 0;
  peak = 0;
}
template <typename V>
auto ObjectPool<V>::GetUsage() const -> PoolUsage {
  return {.inUse = inUse, .available = pool.size(), .keys = 1};
}
template <typename V>
auto ObjectPool<V>::Reallocate(V &) -> void {}
template <typename V>
auto ObjectPool<V>::Deallocate(V &) -> void {}
} // namespace kuki
