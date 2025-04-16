#include <component/mesh.hpp>
#include <glm/ext/vector_float3.hpp>
#include <gtest/gtest.h>
#include <string>
#include <utility/octree.hpp>
#include <utility/trie.hpp>
using namespace kuki;
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
TEST(OctreeTest, OctreeInsert) {
  Octree<unsigned int> octree(glm::vec3(.0f), glm::vec3(10.0f), 3, 2, 4);
  auto result = octree.Insert(0, BoundingBox(glm::vec3(-5.5f), glm::vec3(8.5f)));
  EXPECT_EQ(result, true);
  result = octree.Insert(1, BoundingBox(glm::vec3(-10.0f), glm::vec3(10.0f)));
  EXPECT_EQ(result, true);
  result = octree.Insert(2, BoundingBox(glm::vec3(8.0f), glm::vec3(12.0f)));
  EXPECT_EQ(result, false);
  result = octree.Insert(3, BoundingBox(glm::vec3(-10.0f), glm::vec3(-7.0f)));
  EXPECT_EQ(result, true);
  octree.Insert(4, BoundingBox(glm::vec3(-5.5f), glm::vec3(-2.3f)));
  octree.Insert(5, BoundingBox(glm::vec3(7.4f), glm::vec3(7.5f)));
  octree.Insert(6, BoundingBox(glm::vec3(-4.2f), glm::vec3(-3.3f)));
  octree.Insert(7, BoundingBox(glm::vec3(1.1f), glm::vec3(3.0f)));
  octree.Insert(8, BoundingBox(glm::vec3(2.0f), glm::vec3(2.5f)));
  octree.Insert(9, BoundingBox(glm::vec3(.7f), glm::vec3(1.7f)));
  result = octree.Insert(10, BoundingBox(glm::vec3(3.9f), glm::vec3(5.4f)));
  EXPECT_EQ(result, true);
  //std::cout << octree.ToString() << std::endl;
}
int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
