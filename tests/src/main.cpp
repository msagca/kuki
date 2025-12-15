#include <glm/ext/vector_float3.hpp>
#include <gtest/gtest.h>
#include <id.hpp>
#include <mesh.hpp>
#include <octree.hpp>
#include <string>
#include <trie.hpp>
using namespace kuki;
TEST(TrieTest, TestInsertDelete) {
  Trie<SuffixNode> trie;
  std::string word = "Cube";
  trie.Insert(word);
  auto found = trie.FindWord("Cube");
  EXPECT_EQ(found, true);
  word = "Cube12";
  trie.Insert(word);
  found = trie.FindWord("Cube1");
  EXPECT_EQ(found, false);
  found = trie.FindWord("Cube12");
  EXPECT_EQ(found, true);
  trie.Remove("Cube12");
  found = trie.FindWord("Cube12");
  EXPECT_EQ(found, false);
  trie.Clear();
}
TEST(TrieTest, TestInsertDuplicate) {
  Trie<SuffixNode> trie;
  std::string word = "Cube";
  trie.Insert(word);
  trie.Insert(word);
  EXPECT_EQ(word, "Cube0");
  word = "Cube";
  trie.Insert(word);
  EXPECT_EQ(word, "Cube1");
  trie.Clear();
}
TEST(OctreeTest, OctreeInsert) {
  Octree<ID> octree(glm::vec3(.0f), glm::vec3(10.f), 3, 2, 4);
  auto result = octree.Insert(ID::Generate(), BoundingBox(glm::vec3(-5.5f), glm::vec3(8.5f)));
  EXPECT_EQ(result, true);
  result = octree.Insert(ID::Generate(), BoundingBox(glm::vec3(-10.f), glm::vec3(10.f)));
  EXPECT_EQ(result, true);
  result = octree.Insert(ID::Generate(), BoundingBox(glm::vec3(8.f), glm::vec3(12.f)));
  EXPECT_EQ(result, false);
  result = octree.Insert(ID::Generate(), BoundingBox(glm::vec3(-10.f), glm::vec3(-7.f)));
  EXPECT_EQ(result, true);
  octree.Insert(ID::Generate(), BoundingBox(glm::vec3(-5.5f), glm::vec3(-2.3f)));
  octree.Insert(ID::Generate(), BoundingBox(glm::vec3(7.4f), glm::vec3(7.5f)));
  octree.Insert(ID::Generate(), BoundingBox(glm::vec3(-4.2f), glm::vec3(-3.3f)));
  octree.Insert(ID::Generate(), BoundingBox(glm::vec3(1.1f), glm::vec3(3.f)));
  octree.Insert(ID::Generate(), BoundingBox(glm::vec3(2.f), glm::vec3(2.5f)));
  octree.Insert(ID::Generate(), BoundingBox(glm::vec3(.7f), glm::vec3(1.7f)));
  result = octree.Insert(ID::Generate(), BoundingBox(glm::vec3(3.9f), glm::vec3(5.4f)));
  EXPECT_EQ(result, true);
}
int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
