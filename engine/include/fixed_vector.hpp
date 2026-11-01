#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
namespace kuki {
/// @brief Up to `N` of something, held in place: what a component holds instead of a `std::vector`
/// where the number is small and has a ceiling worth stating, such as an animator's goals.
///
/// Trivially copyable when `T` is, which a vector never is. Adding past the ceiling is refused
/// (`push_back` answers false) rather than growing, so a caller finds out where the limit is met.
template <typename T, size_t N>
struct FixedVector {
  static_assert(N > 0 && N <= UINT8_MAX, "a FixedVector holds between one and 255 elements");
  std::array<T, N> items{};
  uint8_t count{};
  constexpr auto size() const -> size_t {
    return count;
  }
  static constexpr auto capacity() -> size_t {
    return N;
  }
  constexpr auto empty() const -> bool {
    return count == 0;
  }
  constexpr auto full() const -> bool {
    return count == N;
  }
  constexpr auto begin(this auto &self) {
    return self.items.begin();
  }
  constexpr auto end(this auto &self) {
    return self.items.begin() + self.count;
  }
  constexpr auto operator[](this auto &self, const size_t index) -> decltype(auto) {
    return self.items[index];
  }
  constexpr auto push_back(const T &item) -> bool {
    if (full())
      return false;
    items[count++] = item;
    return true;
  }
  constexpr auto erase(const typename std::array<T, N>::const_iterator at) -> void {
    const auto index = static_cast<size_t>(at - items.cbegin());
    if (index >= count)
      return;
    std::move(items.begin() + index + 1, items.begin() + count, items.begin() + index);
    items[--count] = T{};
  }
  /// @brief Removes every element `predicate` holds for, keeping the order of the rest.
  constexpr auto erase_if(auto &&predicate) -> size_t {
    const auto kept = std::remove_if(items.begin(), items.begin() + count, predicate);
    const auto removed = static_cast<size_t>(items.begin() + count - kept);
    std::fill(kept, items.begin() + count, T{});
    count = static_cast<uint8_t>(count - removed);
    return removed;
  }
  constexpr auto clear() -> void {
    items.fill(T{});
    count = 0;
  }
};
} // namespace kuki
