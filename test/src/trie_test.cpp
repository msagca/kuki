#include <gtest/gtest.h>
#include <string_view>
#include <trie.hpp>
using kuki::SequenceNode;
using kuki::Trie;
using kuki::TrieMatch;
TEST(SequenceTrie, MatchesASequenceKeyByKey) {
  Trie<SequenceNode> sequences;
  ASSERT_TRUE(sequences.Insert(std::string_view("vp")));
  EXPECT_EQ(sequences.Find("v"), TrieMatch::PrefixFound);
  EXPECT_EQ(sequences.Find("vp"), TrieMatch::WordFound);
  EXPECT_EQ(sequences.Find("vx"), TrieMatch::NotFound);
}
// A sequence is done the moment its last key is typed, so one that went on past another could
// never be reached, and one that stopped short of another would cut it off.
TEST(SequenceTrie, RefusesOneThatBeginsAnotherOrThatAnotherBegins) {
  Trie<SequenceNode> sequences;
  ASSERT_TRUE(sequences.Insert(std::string_view("vf")));
  EXPECT_FALSE(sequences.Insert(std::string_view("v")));
  EXPECT_FALSE(sequences.Insert(std::string_view("vfa")));
  EXPECT_TRUE(sequences.Insert(std::string_view("vs")));
}
TEST(SequenceTrie, ForgetsARemovedSequence) {
  Trie<SequenceNode> sequences;
  ASSERT_TRUE(sequences.Insert(std::string_view("vb")));
  ASSERT_TRUE(sequences.Remove("vb"));
  EXPECT_NE(sequences.Find("vb"), TrieMatch::WordFound);
}
