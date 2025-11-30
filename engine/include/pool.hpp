#pragma once
#include <concepts>
#include <functional>
#include <unordered_map>
#include <vector>
namespace kuki {
template <typename T>
concept Hashable = requires(const T& t) {
  { std::hash<T>{}(t) } -> std::convertible_to<std::size_t>;
};
template <Hashable Key, typename Val>
class Pool {
protected:
  std::unordered_map<Key, std::vector<Val>> pool;
  virtual Val Allocate(const Key&) = 0;
  virtual void Reallocate(const Key&, Val&) {}
public:
  Pool() = default;
  virtual ~Pool() = default;
  Val Request(const Key& key) {
    if (auto it = pool.find(key); it != pool.end() && !it->second.empty()) {
      auto val = std::move(it->second.back());
      it->second.pop_back();
      return val;
    }
    return Allocate(key);
  }
  template <std::convertible_to<Val>... Vals>
  void Release(const Key& key, Vals&&... vals) {
    auto& resources = pool[key];
    (resources.push_back(std::forward<Vals>(vals)), ...);
  }
  virtual void Clear() {
    pool.clear();
  }
  void PreAllocate(const Key& key, std::size_t count) {
    auto& resources = pool[key];
    resources.reserve(resources.size() + count);
    for (auto i = 0; i < count; ++i)
      resources.push_back(Allocate(key));
  }
};
} // namespace kuki
