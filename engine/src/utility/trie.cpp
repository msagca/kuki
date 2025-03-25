#include <list>
#include <string>
#include <utility/trie.hpp>
Trie::Trie() {
  root = new TrieNode();
}
void Trie::Insert(const std::string& word) {
  if (word.empty())
    return;
  auto node = root;
  for (auto& c : word) {
    auto it = node->children.find(c);
    if (it == node->children.end())
      node->children[c] = new TrieNode();
    node = node->children[c];
  }
  node->wordEnd = true;
}
void Trie::Insert(std::string& word) {
  if (word.empty())
    return;
  auto node = root;
  for (auto& c : word) {
    auto it = node->children.find(c);
    if (it == node->children.end())
      node->children[c] = new TrieNode();
    node = node->children[c];
  }
  if (!node->wordEnd) {
    node->wordEnd = true;
    return;
  }
  auto suffix = std::to_string(node->numSuffix++);
  InsertAt(suffix, node);
  word += suffix;
}
void Trie::InsertAt(const std::string& word, TrieNode* node) {
  if (word.empty() || !node)
    return;
  auto current = node;
  for (auto& c : word) {
    auto it = current->children.find(c);
    if (it == current->children.end())
      current->children[c] = new TrieNode();
    current = current->children[c];
  }
  current->wordEnd = true;
}
void Trie::Delete(const std::string& word) {
  if (word.empty())
    return;
  auto node = root;
  for (auto& c : word) {
    auto it = node->children.find(c);
    if (it == node->children.end())
      return;
    node = it->second;
  }
  node->wordEnd = false;
}
void Trie::DeleteAll(TrieNode* node) {
  node->wordEnd = false;
  for (auto [_, child] : node->children)
    DeleteAll(child);
}
void Trie::DeleteAll(const std::string& prefix) {
  if (prefix.empty())
    return;
  auto node = root;
  for (auto& c : prefix) {
    auto it = node->children.find(c);
    if (it == node->children.end())
      return;
    node = it->second;
  }
  DeleteAll(node);
}
bool Trie::Search(const std::string& word) {
  if (word.empty())
    return false;
  auto node = root;
  for (auto& c : word) {
    auto it = node->children.find(c);
    if (it == node->children.end())
      return false;
    node = it->second;
  }
  return node->wordEnd;
}
bool Trie::StartsWith(const std::string& prefix) {
  if (prefix.empty())
    return true;
  auto node = root;
  for (auto& c : prefix) {
    auto it = node->children.find(c);
    if (it == node->children.end())
      return false;
    node = it->second;
  }
  return true;
}
void Trie::Clear() {
  ClearChildren(root);
}
void Trie::ClearChildren(TrieNode* node) {
  if (!node)
    return;
  for (auto& [_, n] : node->children) {
    ClearChildren(n);
    delete n;
  }
  node->children.clear();
}
