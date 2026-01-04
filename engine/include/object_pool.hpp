#pragma once
#include <concepts.hpp>
#include <vector>
namespace kuki {
template <typename V>
class ObjectPool {
public:
  virtual ~ObjectPool() = default;
  auto PreAllocate(const size_t) -> void;
  auto Request() -> V;
  template <std::convertible_to<V>... Vals>
  auto Release(Vals &&...) -> void;
protected:
  std::vector<V> pool;
  virtual auto Allocate() -> V = 0;
  virtual auto Reallocate(V &) -> void {}
};
template <typename V>
auto ObjectPool<V>::PreAllocate(const size_t count) -> void {
  if (count == 0)
    return;
  pool.reserve(pool.size() + count);
  for (auto i = 0; i < count; ++i)
    pool.push_back(Allocate());
}
template <typename V>
auto ObjectPool<V>::Request() -> V {
  if (pool.size() > 0) {
    auto val = std::move(pool.back());
    pool.pop_back();
    return val;
  }
  return Allocate();
}
template <typename V>
template <std::convertible_to<V>... Vals>
auto ObjectPool<V>::Release(Vals &&...vals) -> void {
  (pool.push_back(std::forward<Vals>(vals)), ...);
}
} // namespace kuki
