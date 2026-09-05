#pragma once
#include <algorithm>
#include <concepts.hpp>
#include <cstddef>
#include <pool_policy.hpp>
#include <unordered_map>
#include <vector>
namespace kuki {
/// @brief Reusable resources grouped by the description they were made to, with a free list each.
///
/// The key is what the resource *is* -- a size, a target description -- so two callers wanting the
/// same thing share a list and a caller wanting something different gets its own. That is what makes
/// the pool useful and also what makes it grow: every distinct key ever asked for opens a list, and
/// dragging a viewport edge asks for a different one on the way past every width it crosses.
///
/// So the pool keeps books. Every key knows how many of its resources are lent out, how many are
/// waiting, and how busy it was at its busiest in the current round. `Collect` reads those and
/// releases what the recent past says will not be wanted again -- see there for the policy.
template <IsHashable K, typename V>
class KeyedPool {
public:
  virtual ~KeyedPool() = default;
  auto PreAllocate(const K &, const size_t) -> void;
  auto Request(const K &) -> V;
  template <std::convertible_to<V>... Vals>
  auto Release(const K &, Vals &&...) -> void;
  /// @brief Moves one resource's accounting from one key to another, for a caller that reallocated
  /// it in place rather than giving it back and asking for another.
  ///
  /// Without this the books quietly stop describing the world. A render target that is resized keeps
  /// the same texture name and has its storage remade at the new size, so the pool goes on counting
  /// it against the size it was born at -- a key that will never be released, never fall idle, and
  /// never be collected, one per resolution the window has ever been.
  auto Rekey(const K &, const K &) -> void;
  /// @brief Releases what the recent past says will not be wanted again. One round.
  ///
  /// Two rules, and both only ever touch the free lists -- what is lent out is untouchable, so this
  /// cannot free something from under its user the way a collector that had to prove reachability
  /// might.
  ///
  /// A key that was used this round keeps enough to reach its own busiest moment, counting what is
  /// already lent out: if the peak was three and two are out, one waits and the rest go. A key that
  /// was untouched keeps everything for `POOL_IDLE_ROUNDS` and is then released whole and forgotten,
  /// which is how a size nobody asks for any more stops costing anything.
  ///
  /// @return How many resources were released, for the caller to report.
  auto Collect() -> size_t;
  /// @brief Releases everything, lent out or not. For teardown, while the API is still alive.
  auto Clear() -> void;
  auto GetUsage() const -> PoolUsage;
protected:
  /// @brief One key's free list and its accounting.
  ///
  /// `peak` is the highest `inUse` reached since the last collection, not since the pool was made.
  /// A high-water mark that never came down would mean one busy moment kept its resources forever,
  /// which is the leak this is meant to answer rather than a way of measuring it.
  struct Bucket {
    std::vector<V> available;
    size_t inUse{};
    size_t peak{};
    size_t idleRounds{};
  };
  std::unordered_map<K, Bucket> pool;
  virtual auto Allocate(const K &) -> V = 0;
  virtual auto Reallocate(const K &, V &) -> void;
  /// @brief Hands one resource back to the graphics API. Overridden by pools that own real objects.
  ///
  /// Deliberately not called from the destructor. Releasing these needs a live context, and by the
  /// time a pool held by a renderer is destroyed there may not be one -- so teardown is `Clear`,
  /// called while the caller still knows the context is good.
  virtual auto Deallocate(V &) -> void;
};
template <IsHashable K, typename V>
auto KeyedPool<K, V>::PreAllocate(const K &key, const size_t count) -> void {
  if (count == 0)
    return;
  auto &bucket = pool[key];
  bucket.available.reserve(bucket.available.size() + count);
  for (size_t i = 0; i < count; ++i)
    bucket.available.emplace_back(Allocate(key));
}
template <IsHashable K, typename V>
auto KeyedPool<K, V>::Request(const K &key) -> V {
  auto &bucket = pool[key];
  bucket.idleRounds = 0;
  ++bucket.inUse;
  bucket.peak = std::max(bucket.peak, bucket.inUse);
  // The entry stays even once its list is empty, where it used to be erased. What is lent out is
  // counted here and nowhere else, so erasing on the way to zero would lose the count of everything
  // currently outstanding -- and a key with nothing waiting and everything lent is precisely the
  // busy case collection must not mistake for an abandoned one.
  if (bucket.available.empty())
    return Allocate(key);
  auto val = std::move(bucket.available.back());
  bucket.available.pop_back();
  return val;
}
template <IsHashable K, typename V>
template <std::convertible_to<V>... Vals>
auto KeyedPool<K, V>::Release(const K &key, Vals &&...vals) -> void {
  auto &bucket = pool[key];
  (bucket.available.push_back(std::forward<Vals>(vals)), ...);
  // Floored rather than wrapped. A resource handed in under a key it was not requested under is a
  // caller's mistake, and one worth surviving as a slightly pessimistic count rather than as a
  // count near the top of its range that reads as an enormous number of outstanding resources.
  bucket.inUse -= std::min(bucket.inUse, sizeof...(Vals));
}
template <IsHashable K, typename V>
auto KeyedPool<K, V>::Rekey(const K &from, const K &to) -> void {
  if (from == to)
    return;
  if (auto it = pool.find(from); it != pool.end() && it->second.inUse > 0)
    --it->second.inUse;
  auto &bucket = pool[to];
  ++bucket.inUse;
  bucket.peak = std::max(bucket.peak, bucket.inUse);
  bucket.idleRounds = 0;
}
template <IsHashable K, typename V>
auto KeyedPool<K, V>::Collect() -> size_t {
  size_t freed = 0;
  for (auto it = pool.begin(); it != pool.end();) {
    auto &bucket = it->second;
    if (bucket.inUse == 0 && bucket.peak == 0) {
      if (++bucket.idleRounds < POOL_IDLE_ROUNDS) {
        ++it;
        continue;
      }
      for (auto &val : bucket.available) {
        Deallocate(val);
        ++freed;
      }
      it = pool.erase(it);
      continue;
    }
    bucket.idleRounds = 0;
    const auto keep = bucket.peak > bucket.inUse ? bucket.peak - bucket.inUse : 0;
    while (bucket.available.size() > keep) {
      Deallocate(bucket.available.back());
      bucket.available.pop_back();
      ++freed;
    }
    // Carried down to what is still out rather than to nothing, so the next round measures the peak
    // above a floor that is already true instead of climbing back up to it from zero.
    bucket.peak = bucket.inUse;
    ++it;
  }
  return freed;
}
template <IsHashable K, typename V>
auto KeyedPool<K, V>::Clear() -> void {
  for (auto &[key, bucket] : pool)
    for (auto &val : bucket.available)
      Deallocate(val);
  pool.clear();
}
template <IsHashable K, typename V>
auto KeyedPool<K, V>::GetUsage() const -> PoolUsage {
  PoolUsage usage{.keys = pool.size()};
  for (const auto &[key, bucket] : pool) {
    usage.inUse += bucket.inUse;
    usage.available += bucket.available.size();
  }
  return usage;
}
template <IsHashable K, typename V>
auto KeyedPool<K, V>::Reallocate(const K &, V &) -> void {}
template <IsHashable K, typename V>
auto KeyedPool<K, V>::Deallocate(V &) -> void {}
} // namespace kuki
