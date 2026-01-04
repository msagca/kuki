#pragma once
#include <concepts.hpp>
#include <functional>
#include <kuki_engine_export.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <utility>
namespace kuki {
template <typename T>
struct TrieNode {
  // TODO: add unicode support
  std::unordered_map<unsigned char, std::unique_ptr<T>> children;
  /// @brief Indicates if this node terminates a valid word
  bool last{};
};
struct SuffixNode final : TrieNode<SuffixNode> {
  /// @brief Stores the next available number suffix that can be appended to words ending at this node to make them unique
  size_t suffix{};
};
using InputAction = std::function<void()>;
struct ActionNode final : TrieNode<ActionNode> {
  /// @brief The action to execute when this node is reached during traversal
  InputAction action{};
};
template <IsTrieNode T>
class KUKI_ENGINE_API Trie {
public:
  Trie();
  Trie &operator=(Trie<T> &&) noexcept = default;
  Trie &operator=(const Trie<T> &) = delete;
  Trie(Trie<T> &&) noexcept = default;
  Trie(const Trie<T> &) = delete;
  /// @brief Insert a word, overwrite duplicates
  /// @return `true` if the word did not already exist, `false` otherwise
  auto Insert(std::string_view) -> bool
    requires(!IsSuffixNode<T> && !IsActionNode<T>);
  /// @brief Insert a word, add a suffix if duplicate, modify the input string to include the suffix (if applicable)
  /// @return `true` if the word was inserted (with or without a suffix), `false` otherwise
  auto Insert(std::string &) -> bool
    requires(IsSuffixNode<T> && !IsActionNode<T>);
  /// @brief Insert a trigger (key sequence) and an associated action to execute
  /// @return `true` if the trigger did not contain or was not a prefix of another trigger, `false` otherwise
  auto Insert(std::string_view, InputAction) -> bool
    requires(!IsSuffixNode<T> && IsActionNode<T>);
  /// @brief Delete the given word if it's in the trie
  auto Remove(std::string_view) -> bool;
  /// @brief Delete all the words that start with the given prefix
  auto RemovePrefix(std::string_view) -> bool;
  auto FindWord(std::string_view) -> bool;
  template <IsCharIterator I>
  auto FindWord(I, I) -> bool;
  auto FindPrefix(std::string_view) -> bool;
  template <IsCharIterator I>
  auto FindPrefix(I, I) -> bool;
  /// @brief Execute a function for each word starting with the given prefix
  template <typename F>
  auto ForEach(std::string_view, F &&) -> void;
private:
  std::unique_ptr<T> root;
  const size_t maxInsertAttempts{8};
  /// @brief Insert a word starting at a given node
  /// @return `true` if no duplicates, `false` otherwise
  auto InsertAt(std::string_view, T &) -> bool
    requires(IsSuffixNode<T> && !IsActionNode<T>);
  auto Invalidate(T &) -> void;
  /// @brief Execute a function for each word starting at the given node
  template <typename F>
  auto ForEach(const T &, std::string &, F &&) -> void;
};
template <IsTrieNode T>
Trie<T>::Trie()
  : root(std::make_unique<T>()) {}
template <IsTrieNode T>
auto Trie<T>::Insert(std::string_view word) -> bool
  requires(!IsSuffixNode<T> && !IsActionNode<T>)
{
  if (word.empty())
    return false;
  auto node = root.get();
  for (const auto &c : word) {
    const auto uc = static_cast<unsigned char>(c);
    auto &child = node->children[uc];
    if (!child)
      child = std::make_unique<T>();
    node = child.get();
  }
  if (node->last)
    return false;
  node->last = true;
  return true;
}
template <IsTrieNode T>
auto Trie<T>::Insert(std::string &word) -> bool
  requires(IsSuffixNode<T> && !IsActionNode<T>)
{
  if (word.empty())
    return false;
  auto node = root.get();
  for (const auto &c : word) {
    const auto uc = static_cast<unsigned char>(c);
    auto &child = node->children[uc];
    if (!child)
      child = std::make_unique<T>();
    node = child.get();
  }
  if (!node->last) {
    node->last = true;
    return true;
  }
  for (auto k = 0; k < maxInsertAttempts; ++k) {
    const auto suffix = std::to_string(node->suffix++);
    if (InsertAt(suffix, node)) {
      word += suffix;
      return true;
    }
  }
  return false;
}
template <IsTrieNode T>
auto Trie<T>::Insert(std::string_view trigger, InputAction action) -> bool
  requires(!IsSuffixNode<T> && IsActionNode<T>)
{
  if (trigger.empty() || !action)
    return false;
  auto node = root.get();
  for (const auto &c : trigger) {
    const auto uc = static_cast<unsigned char>(c);
    auto &child = node->children[uc];
    if (!child)
      child = std::make_unique<T>();
    else if (child->last || child->action)
      // NOTE: a subsequence already exists, abort
      return false;
    node = child.get();
  }
  if (!node->children.empty())
    // NOTE: trigger is a prefix of a longer sequence, abort
    return false;
  node->last = true;
  node->action = std::move(action);
  return true;
}
template <IsTrieNode T>
auto Trie<T>::InsertAt(std::string_view word, T &node) -> bool
  requires(IsSuffixNode<T> && !IsActionNode<T>)
{
  if (word.empty())
    return false;
  auto &current = node;
  for (const auto &c : word) {
    const auto uc = static_cast<unsigned char>(c);
    auto &child = current.children[uc];
    if (!child)
      child = std::make_unique<T>();
    current = *child;
  }
  if (current.last)
    return false;
  current.last = true;
  return true;
}
template <IsTrieNode T>
auto Trie<T>::Remove(std::string_view word) -> bool {
  if (word.empty())
    return false;
  auto node = root.get();
  for (auto &c : word)
    if (auto it = node->children.find(c); it != node->children.end())
      node = it->second.get();
    else
      return false;
  node->last = false;
  return true;
}
template <IsTrieNode T>
auto Trie<T>::Invalidate(T &node) -> void {
  node.last = false;
  for (auto &[_, child] : node.children)
    Invalidate(child);
}
template <IsTrieNode T>
auto Trie<T>::RemovePrefix(std::string_view prefix) -> bool {
  if (prefix.empty())
    return false;
  auto node = root.get();
  for (const auto &c : prefix) {
    const auto uc = static_cast<unsigned char>(c);
    if (auto it = node->children.find(uc); it != node->children.end())
      node = it->second.get();
    else
      return false;
  }
  Invalidate(node);
  return true;
}
template <IsTrieNode T>
auto Trie<T>::FindWord(std::string_view word) -> bool {
  return FindWord(word.begin(), word.end());
}
template <IsTrieNode T>
template <IsCharIterator I>
auto Trie<T>::FindWord(I begin, I end) -> bool {
  auto node = root.get();
  for (auto it = begin; it != end; ++it)
    if (auto it2 = node->children.find(*it); it2 != node->children.end())
      node = it2->second.get();
    else
      return false;
  if constexpr (IsActionNode<T>)
    if (node->action) {
      spdlog::info("Firing action for trigger {}", std::string(begin, end));
      node->action();
    }
  return node->last;
}
template <IsTrieNode T>
auto Trie<T>::FindPrefix(std::string_view prefix) -> bool {
  return FindPrefix(prefix.begin(), prefix.end());
}
template <IsTrieNode T>
template <IsCharIterator I>
auto Trie<T>::FindPrefix(I begin, I end) -> bool {
  auto node = root.get();
  for (auto it = begin; it != end; ++it)
    if (auto it2 = node->children.find(*it); it2 != node->children.end())
      node = it2->second.get();
    else
      return false;
  return true;
}
template <IsTrieNode T>
template <typename F>
auto Trie<T>::ForEach(std::string_view prefix, F &&func) -> void {
  // FIXME: this is too slow when the number of entries is large (e.g., 10K), the app may hang while executing this
  auto node = root.get();
  for (const auto &c : prefix) {
    const auto uc = static_cast<unsigned char>(c);
    if (auto it = node->children.find(uc); it != node->children.end())
      node = it->second.get();
    else
      return;
  }
  ForEach(node, prefix, std::forward<F>(func));
}
template <IsTrieNode T>
template <typename F>
auto Trie<T>::ForEach(const T &node, std::string &buffer, F &&func) -> void {
  if (node.last)
    func(buffer);
  for (const auto &[c, child] : node.children) {
    buffer.push_back(static_cast<unsigned char>(c));
    ForEach(child, buffer, std::forward<F>(func));
    buffer.pop_back();
  }
}
} // namespace kuki
