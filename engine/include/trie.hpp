#pragma once
#include <functional>
#include <kuki_engine_export.h>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <utility>
namespace kuki {
template <typename T>
struct KUKI_ENGINE_API TrieNode {
  // TODO: add unicode support
  std::unordered_map<unsigned char, T*> children;
  /// @brief Whether this node terminates a valid word
  bool last{false};
};
struct KUKI_ENGINE_API SuffixNode final : TrieNode<SuffixNode> {
  /// @brief Stores the next available number suffix that can be appended to words ending at this node to make them unique
  size_t suffix{0};
};
using InputAction = std::function<void()>;
struct KUKI_ENGINE_API ActionNode final : TrieNode<ActionNode> {
  /// @brief The action to execute when this node is reached during traversal
  InputAction action{nullptr};
};
template <typename T>
concept IsTrieNode = std::is_base_of_v<TrieNode<T>, T>;
template <typename T>
concept IsSuffixNode = std::is_base_of_v<SuffixNode, T>;
template <typename T>
concept IsActionNode = std::is_base_of_v<ActionNode, T>;
template <typename T>
concept IsCharLike = std::is_same_v<T, char> || std::is_same_v<T, unsigned char>;
template <typename InputIt>
concept IsCharIterator = std::input_iterator<InputIt> && IsCharLike<std::iter_value_t<InputIt>>;
template <IsTrieNode T>
class KUKI_ENGINE_API Trie {
private:
  T* root;
  const size_t maxInsertAttempts{8};
  /// @brief Insert a word starting at a given node
  /// @returns true if no duplicates, false otherwise
  bool InsertAt(const std::string&, T*)
    requires(IsSuffixNode<T> && !IsActionNode<T>);
  void RemoveAll(T*);
  void ClearChildren(T*);
  /// @brief Execute a function for each word starting at the given node
  template <typename F>
  void ForEach(const T*, std::string&, F);
public:
  Trie()
    : root(new T()) {}
  ~Trie() {
    Clear();
  }
  Trie(const Trie<T>&) = delete;
  Trie& operator=(const Trie<T>&) = delete;
  Trie(Trie<T>&& other) noexcept
    : root(std::exchange(other.root, nullptr)) {}
  Trie& operator=(Trie<T>&& other) noexcept {
    if (this != &other) {
      Clear();
      root = std::exchange(other.root, nullptr);
    }
    return *this;
  }
  /// @brief Insert a word, overwrite duplicates
  /// @returns true if the word did not already exist, false otherwise
  bool Insert(const std::string&)
    requires(!IsSuffixNode<T> && !IsActionNode<T>);
  /// @brief Insert a word, add a suffix if duplicate, modify the input string to include the suffix (if applicable)
  /// @returns true if the word was inserted (with or without a suffix), false otherwise
  bool Insert(std::string&)
    requires(IsSuffixNode<T> && !IsActionNode<T>);
  /// @brief Insert a trigger (key sequence) and an associated action to execute
  /// @returns true if the trigger did not contain or was not a prefix of another trigger, false otherwise
  bool Insert(const std::string&, InputAction)
    requires(!IsSuffixNode<T> && IsActionNode<T>);
  /// @brief Delete the given word if it's in the trie
  bool Remove(const std::string&);
  /// @brief Delete all the words that start with the given prefix
  bool RemovePrefix(const std::string&);
  bool FindWord(const std::string&);
  template <IsCharIterator It>
  bool FindWord(It, It);
  bool FindPrefix(const std::string&);
  template <IsCharIterator It>
  bool FindPrefix(It, It);
  /// @brief Execute a function for each word starting with the given prefix
  template <typename F>
  void ForEach(const std::string&, F);
  /// @brief Release the memory occupied by trie nodes
  void Clear();
};
template <IsTrieNode T>
bool Trie<T>::Insert(const std::string& word)
  requires(!IsSuffixNode<T> && !IsActionNode<T>)
{
  if (word.empty())
    return false;
  auto node = root;
  for (const auto& c : word) {
    auto& child = node->children[c];
    if (!child)
      child = new T();
    node = child;
  }
  if (node->last)
    return false;
  node->last = true;
  return true;
}
template <IsTrieNode T>
bool Trie<T>::Insert(std::string& word)
  requires(IsSuffixNode<T> && !IsActionNode<T>)
{
  if (word.empty())
    return false;
  auto node = root;
  for (const auto& c : word) {
    auto& child = node->children[c];
    if (!child)
      child = new T();
    node = child;
  }
  if (!node->last) {
    node->last = true;
    return true;
  }
  for (auto k = 0; k < maxInsertAttempts; ++k) {
    auto suffix = std::to_string(node->suffix++);
    if (InsertAt(suffix, node)) {
      word += suffix;
      return true;
    }
  }
  return false;
}
template <IsTrieNode T>
bool Trie<T>::Insert(const std::string& trigger, InputAction action)
  requires(!IsSuffixNode<T> && IsActionNode<T>)
{
  if (trigger.empty() || !action)
    return false;
  auto node = root;
  for (const auto& c : trigger) {
    auto& child = node->children[c];
    if (!child)
      child = new T();
    else if (child->last || child->action) {
      assert(child->last && child->action);
      // NOTE: a subsequence already exists, abort
      return false;
    }
    node = child;
  }
  if (!node->children.empty())
    // NOTE: trigger is a prefix of a longer sequence, abort
    return false;
  node->last = true;
  node->action = std::move(action);
  return true;
}
template <IsTrieNode T>
bool Trie<T>::InsertAt(const std::string& word, T* node)
  requires(IsSuffixNode<T> && !IsActionNode<T>)
{
  if (word.empty() || !node)
    return false;
  auto current = node;
  for (const auto& c : word) {
    auto& child = current->children[c];
    if (!child)
      child = new T();
    current = child;
  }
  if (current->last)
    return false;
  current->last = true;
  return true;
}
template <IsTrieNode T>
bool Trie<T>::Remove(const std::string& word) {
  if (word.empty())
    return false;
  auto node = root;
  for (auto& c : word) {
    auto it = node->children.find(c);
    if (it == node->children.end())
      return false;
    node = it->second;
  }
  node->last = false;
  return true;
}
template <IsTrieNode T>
void Trie<T>::RemoveAll(T* node) {
  node->last = false;
  for (auto& [_, child] : node->children)
    RemoveAll(child);
}
template <IsTrieNode T>
bool Trie<T>::RemovePrefix(const std::string& prefix) {
  if (prefix.empty())
    return false;
  auto node = root;
  for (const auto& c : prefix) {
    auto it = node->children.find(c);
    if (it == node->children.end())
      return false;
    node = it->second;
  }
  RemoveAll(node);
  return true;
}
template <IsTrieNode T>
bool Trie<T>::FindWord(const std::string& word) {
  return FindWord(word.begin(), word.end());
}
template <IsTrieNode T>
template <IsCharIterator It>
bool Trie<T>::FindWord(It begin, It end) {
  auto node = root;
  for (auto wit = begin; wit != end; ++wit) {
    auto cit = node->children.find(*wit);
    if (cit == node->children.end())
      return false;
    node = cit->second;
  }
  if constexpr (IsActionNode<T>)
    if (node->action) {
      assert(node->last);
      spdlog::info("Firing action for trigger '{}'.", std::string(begin, end));
      node->action();
    }
  return node->last;
}
template <IsTrieNode T>
bool Trie<T>::FindPrefix(const std::string& prefix) {
  return FindPrefix(prefix.begin(), prefix.end());
}
template <IsTrieNode T>
template <IsCharIterator It>
bool Trie<T>::FindPrefix(It begin, It end) {
  auto node = root;
  for (auto wit = begin; wit != end; ++wit) {
    auto cit = node->children.find(*wit);
    if (cit == node->children.end())
      return false;
    node = cit->second;
  }
  return true;
}
template <IsTrieNode T>
void Trie<T>::Clear() {
  ClearChildren(root);
  delete root;
  root = nullptr;
}
template <IsTrieNode T>
void Trie<T>::ClearChildren(T* node) {
  if (!node)
    return;
  for (auto& [_, n] : node->children) {
    ClearChildren(n);
    delete n;
  }
  node->children.clear();
}
template <IsTrieNode T>
template <typename F>
void Trie<T>::ForEach(const std::string& prefix, F func) {
  // FIXME: this is too slow when the number of entries is large (e.g., 10K), the app may hang while executing this
  auto node = root;
  for (const auto& c : prefix)
    if (auto it = node->children.find(c); it != node->children.end())
      node = it->second;
    else
      return;
  std::string buffer = prefix;
  ForEach(node, buffer, func);
}
template <IsTrieNode T>
template <typename F>
void Trie<T>::ForEach(const T* node, std::string& buffer, F func) {
  if (node->last)
    func(buffer);
  for (const auto& [c, child] : node->children) {
    buffer.push_back(c);
    ForEach(child, buffer, func);
    buffer.pop_back();
  }
}
} // namespace kuki
