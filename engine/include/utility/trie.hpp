#include <engine_export.h>
#include <unordered_map>
#include <string>
struct ENGINE_API TrieNode {
  std::unordered_map<char, TrieNode*> children;
  /// @brief The next number suffix available that can be appended to this node
  unsigned int numSuffix = 0;
  bool wordEnd = false;
};
class ENGINE_API Trie {
private:
  TrieNode* root;
  void InsertAt(const std::string&, TrieNode*);
  void DeleteAll(TrieNode*);
  void ClearChildren(TrieNode*);
  /// @brief Execute a function for each word starting at the given node
  template <typename F>
  void ForEach(TrieNode*, const std::string&, F);
public:
  Trie();
  void Insert(const std::string&);
  /// @brief Insert the word, add a suffix if it's a duplicate, modify the input string to reflect the change
  void Insert(std::string&);
  void Delete(const std::string&);
  /// @brief Delete all the words that start with the given prefix
  void DeleteAll(const std::string&);
  bool Search(const std::string&);
  bool StartsWith(const std::string&);
  /// @brief Execute a function for each word starting with the given prefix
  template <typename F>
  void ForEach(const std::string&, F);
  /// @brief Release the memory occupied by trie nodes
  void Clear();
};
template <typename F>
void Trie::ForEach(const std::string& prefix, F func) {
  auto node = root;
  for (auto& c : prefix) {
    auto it = node->children.find(c);
    if (it == node->children.end())
      return;
    node = it->second;
  }
  ForEach(node, prefix, func);
}
template <typename F>
void Trie::ForEach(TrieNode* node, const std::string& prefix, F func) {
  if (node->wordEnd)
    func(prefix);
  for (auto& [c, child] : node->children)
    ForEach(child, prefix + c, func);
}
