#pragma once
#include <cstddef>
#include <type_traits>
namespace kuki {
/// @brief A list of types, and nothing else: what the component and archetype declarations are
/// written in, so that a question about them -- does this archetype have a transform, does any
/// archetype in the game have both of these -- is answered by the compiler and not at run time.
template <typename... Ts>
struct TypeList {
  static constexpr size_t size = sizeof...(Ts);
};
template <typename List, typename T>
inline constexpr bool Contains = false;
template <typename... Ts, typename T>
inline constexpr bool Contains<TypeList<Ts...>, T> = (std::is_same_v<Ts, T> || ...);
template <typename... Lists>
struct ConcatImpl;
template <>
struct ConcatImpl<> {
  using Type = TypeList<>;
};
template <typename... Ts>
struct ConcatImpl<TypeList<Ts...>> {
  using Type = TypeList<Ts...>;
};
template <typename... As, typename... Bs, typename... Rest>
struct ConcatImpl<TypeList<As...>, TypeList<Bs...>, Rest...> {
  using Type = typename ConcatImpl<TypeList<As..., Bs...>, Rest...>::Type;
};
/// @brief One list holding every type of the lists given, in order.
template <typename... Lists>
using Concat = typename ConcatImpl<Lists...>::Type;
template <typename List>
inline constexpr bool IsUnique = true;
template <typename T, typename... Ts>
inline constexpr bool IsUnique<TypeList<T, Ts...>> = !(std::is_same_v<T, Ts> || ...) && IsUnique<TypeList<Ts...>>;
template <typename Seen, typename Rest>
struct UniqueImpl;
template <typename... Seen>
struct UniqueImpl<TypeList<Seen...>, TypeList<>> {
  using Type = TypeList<Seen...>;
};
template <typename... Seen, typename T, typename... Ts>
struct UniqueImpl<TypeList<Seen...>, TypeList<T, Ts...>> {
  using Type = typename UniqueImpl<std::conditional_t<(std::is_same_v<T, Seen> || ...), TypeList<Seen...>, TypeList<Seen..., T>>, TypeList<Ts...>>::Type;
};
/// @brief The list with each type kept once, at its first place.
template <typename List>
using Unique = typename UniqueImpl<TypeList<>, List>::Type;
template <typename List, template <typename> typename Predicate>
struct FilterImpl;
template <template <typename> typename Predicate>
struct FilterImpl<TypeList<>, Predicate> {
  using Type = TypeList<>;
};
template <typename T, typename... Ts, template <typename> typename Predicate>
struct FilterImpl<TypeList<T, Ts...>, Predicate> {
  using Rest = typename FilterImpl<TypeList<Ts...>, Predicate>::Type;
  using Type = std::conditional_t<Predicate<T>::value, Concat<TypeList<T>, Rest>, Rest>;
};
/// @brief The types of a list for which `Predicate<T>::value` holds, in order.
template <typename List, template <typename> typename Predicate>
using Filter = typename FilterImpl<List, Predicate>::Type;
template <template <typename...> typename To, typename List>
struct ApplyImpl;
template <template <typename...> typename To, typename... Ts>
struct ApplyImpl<To, TypeList<Ts...>> {
  using Type = To<Ts...>;
};
/// @brief The list's types handed to another template: `Apply<std::variant, List>`.
template <template <typename...> typename To, typename List>
using Apply = typename ApplyImpl<To, List>::Type;
template <template <typename> typename F, typename List>
struct MapImpl;
template <template <typename> typename F, typename... Ts>
struct MapImpl<F, TypeList<Ts...>> {
  using Type = TypeList<F<Ts>...>;
};
/// @brief The list with each of its types put through `F`.
template <template <typename> typename F, typename List>
using Map = typename MapImpl<F, List>::Type;
/// @brief Calls `func.template operator()<T>()` for each type of the list, in order.
template <typename... Ts>
constexpr auto ForEachType(TypeList<Ts...>, auto &&func) -> void {
  (func.template operator()<Ts>(), ...);
}
/// @brief Whether `func.template operator()<T>()` is true for any type of the list, stopping at the first.
template <typename... Ts>
constexpr auto AnyType(TypeList<Ts...>, auto &&func) -> bool {
  return (func.template operator()<Ts>() || ...);
}
} // namespace kuki
