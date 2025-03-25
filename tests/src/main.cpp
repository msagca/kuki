#include <gtest/gtest.h>
#include <string>
#include <utility/octree.hpp>
#include <utility/trie.hpp>
#include <component/mesh.hpp>
#include <glm/ext/vector_float3.hpp>
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
  Octree<unsigned int> octree(glm::vec3(.0f), glm::vec3(1.0f), 0, 3, 2, 4);
  auto id = 0;
  auto result = octree.Insert(id++, BoundingBox(glm::vec3(-.5f), glm::vec3(.5f)));
  EXPECT_EQ(result, true);
  result = octree.Insert(id++, BoundingBox(glm::vec3(-1.0f), glm::vec3(1.0f)));
  EXPECT_EQ(result, true);
  result = octree.Insert(id++, BoundingBox(glm::vec3(2.0f), glm::vec3(4.0f)));
  EXPECT_EQ(result, false);
  result = octree.Insert(id++, BoundingBox(glm::vec3(-4.0f), glm::vec3(-2.0f)));
  EXPECT_EQ(result, false);
  octree.Insert(id++, BoundingBox(glm::vec3(-.5f), glm::vec3(-.3f)));
  octree.Insert(id++, BoundingBox(glm::vec3(.4f), glm::vec3(.5f)));
  octree.Insert(id++, BoundingBox(glm::vec3(.2f), glm::vec3(.3f)));
  octree.Insert(id++, BoundingBox(glm::vec3(.1f), glm::vec3(3.0f)));
  octree.Insert(id++, BoundingBox(glm::vec3(2.0f), glm::vec3(2.5f)));
  octree.Insert(id++, BoundingBox(glm::vec3(.7f), glm::vec3(1.7f)));
  result = octree.Insert(id++, BoundingBox(glm::vec3(.9f), glm::vec3(5.4f)));
  EXPECT_EQ(result, true);
  std::cout << octree.ToString() << std::endl;
}
int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
