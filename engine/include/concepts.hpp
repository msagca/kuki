#pragma once
#include <concepts>
#include <functional>
namespace kuki {
template <typename T>
concept IsHashable = requires(const T &t) {
  { std::hash<T>{}(t) } -> std::convertible_to<size_t>;
};
template <typename T>
concept IsUUID = IsHashable<T> && requires {
  { T::Generate() } -> std::same_as<T>;
  { T::Invalid } -> std::same_as<const T &>;
};
class System;
template <typename T>
concept IsSystem = std::is_base_of_v<System, T>;
struct Asset;
template <typename T>
concept IsAsset = std::is_base_of_v<Asset, T>;
template <typename... T>
concept AreUnsignedInt = (std::convertible_to<T, unsigned int> && ...);
template <typename... T>
concept AreSizeT = (std::convertible_to<T, size_t> && ...);
template <typename T>
struct TrieNode;
template <typename T>
concept IsTrieNode = std::is_base_of_v<TrieNode<T>, T>;
struct SuffixNode;
template <typename T>
concept IsSuffixNode = std::is_base_of_v<SuffixNode, T>;
struct ActionNode;
template <typename T>
concept IsActionNode = std::is_base_of_v<ActionNode, T>;
template <typename T>
concept IsCharLike = std::is_same_v<T, char> || std::is_same_v<T, unsigned char>;
template <typename T>
concept IsCharIterator = std::input_iterator<T> && IsCharLike<std::iter_value_t<T>>;
template <typename S, typename T>
using ConstCorrectPointer = std::conditional_t<std::is_const_v<std::remove_reference_t<S>>, const T *, T *>;
template <typename S, typename T>
using ConstCorrectValue = std::conditional_t<std::is_const_v<std::remove_reference_t<S>>, const T, T>;
template <typename S, typename CT, typename T>
using ConstBasedValue = std::conditional_t<std::is_const_v<std::remove_reference_t<S>>, CT, T>;
class Script;
template <typename T>
concept IsScript = std::is_base_of_v<Script, T>;
/// @brief Evaluates to `Script` for the types that extend it, preserves the type otherwise
template <typename T>
using ScriptAwareType = std::conditional_t<std::is_base_of_v<Script, T>, Script, T>;
} // namespace kuki
