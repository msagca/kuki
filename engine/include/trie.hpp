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
  bool last{};
};
struct SuffixNode final : TrieNode<SuffixNode> {
  size_t suffix{};
};
using InputAction = std::function<void()>;
struct ActionNode final : TrieNode<ActionNode> {
  InputAction action{};
};
enum class TrieMatch : uint8_t { NotFound,
  PrefixFound,
  WordFound };
template <IsTrieNode T>
class KUKI_ENGINE_API Trie {
public:
  Trie();
  Trie &operator=(Trie<T> &&) noexcept = default;
  Trie &operator=(const Trie<T> &) = delete;
  Trie(Trie<T> &&) noexcept = default;
  Trie(const Trie<T> &) = delete;
  auto Insert(std::string_view) -> bool
    requires(!IsSuffixNode<T> && !IsActionNode<T>);
  auto Insert(std::string &) -> bool
    requires(IsSuffixNode<T> && !IsActionNode<T>);
  auto Insert(std::string_view, InputAction) -> bool
    requires(!IsSuffixNode<T> && IsActionNode<T>);
  auto Remove(std::string_view) -> bool;
  auto RemovePrefix(std::string_view) -> bool;
  auto Find(std::string_view) -> TrieMatch;
  template <IsCharIterator I>
  auto Find(I, I) -> TrieMatch;
  template <typename F>
  auto ForEach(std::string_view, F &&) -> void;
private:
  std::unique_ptr<T> root;
  const size_t maxInsertAttempts{8};
  auto InsertAt(std::string_view, T &) -> bool
    requires(IsSuffixNode<T> && !IsActionNode<T>);
  auto Invalidate(T &) -> void;
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
      return false;
    node = child.get();
  }
  if (!node->children.empty())
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
  for (const auto &c : word) {
    const auto uc = static_cast<unsigned char>(c);
    if (auto it = node->children.find(uc); it != node->children.end())
      node = it->second.get();
    else
      return false;
  }
  node->last = false;
  if constexpr (IsActionNode<T>)
    node->action = {};
  return true;
}
template <IsTrieNode T>
auto Trie<T>::Invalidate(T &node) -> void {
  node.last = false;
  if constexpr (IsActionNode<T>)
    node.action = {};
  for (auto &[_, child] : node.children)
    Invalidate(*child);
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
  Invalidate(*node);
  return true;
}
template <IsTrieNode T>
auto Trie<T>::Find(std::string_view word) -> TrieMatch {
  return Find(word.begin(), word.end());
}
template <IsTrieNode T>
template <IsCharIterator I>
auto Trie<T>::Find(I begin, I end) -> TrieMatch {
  auto node = root.get();
  for (auto it = begin; it != end; ++it)
    if (auto it2 = node->children.find(*it); it2 != node->children.end())
      node = it2->second.get();
    else
      return TrieMatch::NotFound;
  if constexpr (IsActionNode<T>)
    if (node->action) {
      spdlog::info("Firing action for trigger {}", std::string(begin, end));
      node->action();
    }
  return node->last ? TrieMatch::WordFound : TrieMatch::PrefixFound;
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
