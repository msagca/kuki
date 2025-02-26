#include <gtest/gtest.h>
#include <trie.hpp>
#include <string>
TEST(TrieTest, TestInsertDelete) {
  Trie trie;
  trie.Insert("Cube");
  auto found = trie.Search("Cube");
  EXPECT_EQ(found, true);
  trie.Insert("Cube12");
  found = trie.Search("Cube1");
  EXPECT_EQ(found, false);
  found = trie.Search("Cube12");
  EXPECT_EQ(found, true);
  trie.Delete("Cube12");
  found = trie.Search("Cube12");
  EXPECT_EQ(found, false);
  trie.Clear();
}
TEST(TrieTest, TestInsertDuplicate) {
  Trie trie;
  trie.Insert("Cube");
  std::string word = "Cube";
  trie.Insert(word);
  EXPECT_EQ(word, "Cube0");
  word = "Cube";
  trie.Insert(word);
  EXPECT_EQ(word, "Cube1");
  trie.Clear();
}
int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
