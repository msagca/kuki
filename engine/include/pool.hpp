#pragma once
#include <concepts.hpp>
#include <unordered_map>
#include <vector>
namespace kuki {
template <IsHashable K, typename V>
class Pool {
public:
  virtual ~Pool() = default;
  auto PreAllocate(const K &, const size_t) -> void;
  auto Request(const K &) -> V;
  template <std::convertible_to<V>... Vals>
  auto Release(const K &, Vals &&...) -> void;
protected:
  std::unordered_map<K, std::vector<V>> pool;
  virtual auto Allocate(const K &) -> V = 0;
  virtual auto Reallocate(const K &, V &) -> void {}
};
template <IsHashable K, typename V>
auto Pool<K, V>::PreAllocate(const K &key, const size_t count) -> void {
  if (count == 0)
    return;
  pool[key].reserve(pool[key].size() + count);
  for (auto i = 0; i < count; ++i)
    pool[key].push_back(Allocate(key));
}
template <IsHashable K, typename V>
auto Pool<K, V>::Request(const K &key) -> V {
  if (auto it = pool.find(key); it != pool.end() && !it->second.empty()) {
    auto val = std::move(it->second.back());
    it->second.pop_back();
    return val;
  }
  return Allocate(key);
}
template <IsHashable K, typename V>
template <std::convertible_to<V>... Vals>
auto Pool<K, V>::Release(const K &key, Vals &&...vals) -> void {
  (pool[key].push_back(std::forward<Vals>(vals)), ...);
}
} // namespace kuki
