#include <engine_export.h>
#include <unordered_map>
#include <string>
class ENGINE_API TrieNode {
public:
  std::unordered_map<char, TrieNode*> children;
  /// <summary>
  /// The next number suffix available that can be appended to this node
  /// </summary>
  unsigned int numSuffix = 0;
  bool wordEnd = false;
};
class ENGINE_API Trie {
private:
  TrieNode* root;
  void InsertAt(const std::string&, TrieNode*);
  void ClearChildren(TrieNode*);
public:
  Trie();
  void Insert(const std::string&);
  /// <summary>
  /// Insert the word, add a suffix if it's a duplicate, modify the input string to reflect the change
  /// </summary>
  void Insert(std::string&);
  void Delete(const std::string&);
  bool Search(const std::string&);
  bool StartsWith(const std::string&);
  /// <summary>
  /// Releases the memory occupied by trie nodes
  /// </summary>
  void Clear();
};
