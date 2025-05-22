#pragma once
#include <functional>
namespace kuki {
template <typename TKey, typename TVal>
class Pool {
protected:
  template <typename T>
  static constexpr bool has_std_hash = requires(T key) {
    { std::hash<T>{}(key) } -> std::convertible_to<size_t>;
  };
  template <typename T>
  static constexpr bool has_member_hash = requires {
    typename T::Hash;
    requires std::invocable<typename T::Hash, const T&> && std::convertible_to<std::invoke_result_t<typename T::Hash, const T&>, size_t>;
  };
  static_assert(has_std_hash<TKey> || has_member_hash<TKey>, "TKey must either be hashable by std::hash or provide a Hash member type.");
  using HashType = std::conditional_t<has_std_hash<TKey>,
    std::hash<TKey>,
    typename TKey::Hash>;
  std::unordered_map<TKey, std::vector<TVal>, HashType> pool;
  virtual TVal Allocate(const TKey&) = 0;
  virtual void Reallocate(const TKey& key, TVal& val) {}
public:
  Pool() = default;
  virtual ~Pool() = default;
  TVal Request(const TKey& key) {
    auto it = pool.find(key);
    if (it != pool.end() && !it->second.empty()) {
      auto val = std::move(it->second.back());
      it->second.pop_back();
      return val;
    }
    return Allocate(key);
  }
  template <typename... Vals>
  void Release(const TKey& key, Vals&&... vals) {
    static_assert((std::is_convertible_v<std::decay_t<Vals>, TVal> && ...), "All arguments must be convertible to TVal.");
    auto& resources = pool[key];
    (resources.push_back(std::forward<Vals>(vals)), ...);
  }
  virtual void Clear() {
    pool.clear();
  }
  void PreAllocate(const TKey& key, size_t count) {
    auto& resources = pool[key];
    auto len = resources.size();
    resources.reserve(len + count);
    for (auto i = 0; i < count; ++i)
      resources.push_back(Allocate(key));
  }
};
} // namespace kuki
