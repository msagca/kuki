#include <gesture.hpp>
#include <gtest/gtest.h>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>
using namespace kuki;
namespace {
constexpr ButtonMask A = 1, B = 2, C = 4, M = 8;
const std::vector<std::string> NAMES{"a", "b", "c", "m"};
/// @brief Feeds runs of masks, each for a number of ticks, and collects what was recognised and when.
struct Player {
  GestureRecognizer recognizer;
  std::vector<std::pair<int, GestureID>> seen;
  int tick{};
  auto Add(const std::string &notation, const uint16_t window = Gesture::DEFAULT_WINDOW, const bool strict = false) -> GestureID {
    return recognizer.Add(notation, notation, NAMES, window, strict);
  }
  auto Feed(const std::initializer_list<std::pair<ButtonMask, int>> runs) -> void {
    for (const auto &[mask, ticks] : runs)
      for (auto i = 0; i < ticks; ++i, ++tick)
        for (const auto id : recognizer.Advance(mask))
          seen.emplace_back(tick, id);
  }
  auto Count(const GestureID id) const -> int {
    auto count = 0;
    for (const auto &entry : seen)
      count += entry.second == id;
    return count;
  }
};
} // namespace
TEST(ParseGesture, ReadsEveryForm) {
  const auto steps = ParseGesture("a  b+c a*30 -a m/b m/-a+b", NAMES);
  ASSERT_TRUE(steps);
  ASSERT_EQ(steps->size(), 6u);
  EXPECT_EQ((*steps)[0].buttons, A);
  EXPECT_EQ((*steps)[0].kind, GestureStepKind::Press);
  EXPECT_EQ((*steps)[1].buttons, B | C);
  EXPECT_EQ((*steps)[2].kind, GestureStepKind::Hold);
  EXPECT_EQ((*steps)[2].ticks, 30);
  EXPECT_EQ((*steps)[3].kind, GestureStepKind::Release);
  EXPECT_EQ((*steps)[4].with, M);
  EXPECT_EQ((*steps)[4].buttons, B);
  EXPECT_EQ((*steps)[5].kind, GestureStepKind::Release);
  EXPECT_EQ((*steps)[5].buttons, A | B);
}
TEST(ParseGesture, RefusesWhatItCannotRead) {
  EXPECT_FALSE(ParseGesture("", NAMES));
  EXPECT_FALSE(ParseGesture("z", NAMES));
  EXPECT_FALSE(ParseGesture("a+", NAMES));
  EXPECT_FALSE(ParseGesture("a*", NAMES));
  EXPECT_FALSE(ParseGesture("a*x", NAMES));
  EXPECT_FALSE(ParseGesture("-a*3", NAMES));
}
TEST(GestureRecognizer, RefusesWhatCannotBeMade) {
  GestureRecognizer recognizer;
  EXPECT_EQ(recognizer.Add(Gesture{"empty", {}}), INVALID_GESTURE);
  EXPECT_EQ(recognizer.Add(Gesture{"buttonless", {GestureStep{}}}), INVALID_GESTURE);
  EXPECT_EQ(recognizer.Add("typo", "q", NAMES), INVALID_GESTURE);
  EXPECT_EQ(recognizer.Add("forever", "a*9999", NAMES), INVALID_GESTURE);
}
TEST(GestureRecognizer, SeesAPressOnceHoweverLongItIsHeld) {
  Player player;
  const auto press = player.Add("a");
  player.Feed({{0, 3}, {A, 40}, {0, 3}});
  ASSERT_EQ(player.Count(press), 1);
  EXPECT_EQ(player.seen.front().first, 3);
}
TEST(GestureRecognizer, SeesATapThatLastsOneTick) {
  Player player;
  const auto press = player.Add("a");
  const auto release = player.Add("-a");
  player.Feed({{A, 1}, {0, 1}});
  EXPECT_EQ(player.Count(press), 1);
  EXPECT_EQ(player.Count(release), 1);
}
TEST(GestureRecognizer, SeesASequenceInsideItsWindowOnly) {
  Player player;
  const auto sequence = player.Add("a b", 10);
  player.Feed({{A, 2}, {0, 8}, {B, 2}, {0, 30}});
  EXPECT_EQ(player.Count(sequence), 1);
  player.Feed({{A, 2}, {0, 9}, {B, 2}, {0, 30}});
  EXPECT_EQ(player.Count(sequence), 1);
}
TEST(GestureRecognizer, NeedsTwoPressesForADoubleTap) {
  Player player;
  const auto twice = player.Add("a a");
  player.Feed({{A, 30}, {0, 30}});
  EXPECT_EQ(player.Count(twice), 0);
  player.Feed({{A, 3}, {0, 3}, {A, 3}, {0, 3}});
  EXPECT_EQ(player.Count(twice), 1);
}
TEST(GestureRecognizer, TakesAChordWhoseKeysLandNearlyTogether) {
  Player player;
  const auto chord = player.Add("a+b");
  player.Feed({{A, 2}, {A | B, 5}, {0, 5}});
  EXPECT_EQ(player.Count(chord), 1);
  player.Feed({{A, 20}, {A | B, 5}, {0, 5}});
  EXPECT_EQ(player.Count(chord), 1);
}
TEST(GestureRecognizer, TakesAModifierHoweverLongItHasBeenDown) {
  Player player;
  const auto modified = player.Add("m/a");
  player.Feed({{A, 2}, {0, 2}});
  EXPECT_EQ(player.Count(modified), 0);
  player.Feed({{M, 100}, {M | A, 2}, {0, 2}});
  EXPECT_EQ(player.Count(modified), 1);
}
TEST(GestureRecognizer, ReportsAHoldOnTheTickItIsLongEnough) {
  Player player;
  const auto hold = player.Add("a*20");
  player.Feed({{A, 19}, {0, 5}});
  EXPECT_EQ(player.Count(hold), 0);
  player.Feed({{A, 60}, {0, 5}});
  ASSERT_EQ(player.Count(hold), 1);
  EXPECT_EQ(player.seen.back().first, 24 + 19);
}
TEST(GestureRecognizer, ReleasesAChargeHoweverLongItWasHeld) {
  Player player;
  const auto charge = player.Add("a*20 -a");
  player.Feed({{A, 10}, {0, 5}});
  EXPECT_EQ(player.Count(charge), 0);
  player.Feed({{A, 200}, {0, 5}});
  EXPECT_EQ(player.Count(charge), 1);
}
TEST(GestureRecognizer, ForgivesAStrayPressUnlessStrict) {
  Player player;
  const auto loose = player.Add("a b");
  const auto strict = player.recognizer.Add("strict", "a b", NAMES, Gesture::DEFAULT_WINDOW, true);
  player.Feed({{A, 2}, {0, 1}, {C, 2}, {0, 1}, {B, 2}, {0, 30}});
  EXPECT_EQ(player.Count(loose), 1);
  EXPECT_EQ(player.Count(strict), 0);
  player.Feed({{A, 2}, {0, 1}, {B, 2}, {0, 30}});
  EXPECT_EQ(player.Count(loose), 2);
  EXPECT_EQ(player.Count(strict), 1);
}
TEST(GestureRecognizer, ReportsTheMostSpecificFirst) {
  GestureRecognizer recognizer;
  const auto single = recognizer.Add("single", "b", NAMES);
  const auto sequence = recognizer.Add("sequence", "a b", NAMES);
  recognizer.Advance(A);
  recognizer.Advance(0);
  const auto matched = recognizer.Advance(B);
  ASSERT_EQ(matched.size(), 2u);
  EXPECT_EQ(matched[0], sequence);
  EXPECT_EQ(matched[1], single);
}
TEST(GestureRecognizer, GivesTheSameAnswersForTheSameMasks) {
  Player first, second;
  for (auto *player : {&first, &second}) {
    player->Add("a b c");
    player->Add("a+b");
    player->Add("c*5 -c");
    player->Feed({{A, 2}, {A | B, 3}, {C, 9}, {0, 2}, {A, 1}, {B, 1}, {C, 1}, {0, 4}});
  }
  EXPECT_FALSE(first.seen.empty());
  EXPECT_EQ(first.seen, second.seen);
}
TEST(GestureRecognizer, StartsAgainAfterAReset) {
  Player player;
  const auto sequence = player.Add("a b");
  player.Feed({{A, 2}, {0, 2}});
  player.recognizer.Reset();
  player.Feed({{B, 2}, {0, 2}});
  EXPECT_EQ(player.Count(sequence), 0);
  EXPECT_EQ(player.recognizer.GetHeldTicks(A), 0);
  player.Feed({{A, 7}});
  EXPECT_EQ(player.recognizer.GetHeld(), A);
  EXPECT_EQ(player.recognizer.GetHeldTicks(A), 7);
}
