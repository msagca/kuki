#pragma once
#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
namespace kuki {
/// @brief A name of at most `N - 1` characters, held in place: what a component holds instead of a
/// `std::string`, which would put heap memory in a table row and stop it being copied as bytes.
///
/// One longer than it can hold is cut short rather than refused, so a size is chosen to be generous
/// for what it names: a bone, a goal.
template <size_t N>
struct FixedString {
  static_assert(N > 1, "a FixedString needs room for at least one character and the terminator");
  char data[N]{};
  constexpr FixedString() = default;
  constexpr FixedString(const std::string_view text) {
    Assign(text);
  }
  constexpr FixedString(const char *text)
    : FixedString(std::string_view(text)) {}
  FixedString(const std::string &text)
    : FixedString(std::string_view(text)) {}
  constexpr auto Assign(const std::string_view text) -> void {
    const auto length = std::min(text.size(), N - 1);
    std::copy_n(text.data(), length, data);
    std::fill(data + length, data + N, '\0');
  }
  constexpr auto View() const -> std::string_view {
    return {data, Size()};
  }
  constexpr auto Size() const -> size_t {
    size_t length = 0;
    while (length < N - 1 && data[length] != '\0')
      ++length;
    return length;
  }
  constexpr auto empty() const -> bool {
    return data[0] == '\0';
  }
  constexpr auto c_str() const -> const char * {
    return data;
  }
  constexpr auto clear() -> void {
    std::fill(data, data + N, '\0');
  }
  constexpr operator std::string_view() const {
    return View();
  }
  constexpr auto operator==(const FixedString &other) const -> bool {
    return View() == other.View();
  }
  constexpr auto operator==(const std::string_view other) const -> bool {
    return View() == other;
  }
  constexpr auto operator==(const char *other) const -> bool {
    return View() == std::string_view(other);
  }
  auto operator==(const std::string &other) const -> bool {
    return View() == std::string_view(other);
  }
};
} // namespace kuki
