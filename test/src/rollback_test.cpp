#include <algorithm>
#include <array>
#include <cstdlib>
#include <gtest/gtest.h>
#include <map>
#include <rollback.hpp>
#include <simulated_link.hpp>
using namespace kuki;
namespace {
/// A game with just enough in it to be wrong visibly: two figures that step and strike, damage that
/// depends on a generator, and a digest of everything that has happened, so that a tick run on the
/// wrong buttons -- or run in the wrong order, or not restored exactly -- changes every digest after.
class ToyGame final : public RollbackGame {
public:
  explicit ToyGame(const int64_t corruptAt = -1)
    : corruptAt(corruptAt) {}
  /// The digest after each tick, the latest run of a tick replacing any earlier one. Not part of
  /// the state: it is how the tests see what the game finally decided each tick was.
  std::map<int64_t, uint32_t> trace;
  std::array<int32_t, 2> position{-3, 3};
  std::array<int32_t, 2> life{1000, 1000};
  uint32_t random{12345};
  uint32_t digest{2166136261u};
  auto Advance(const int64_t tick, const std::span<const ButtonMask> buttons, const bool) -> void override {
    for (auto player = 0; player < 2; ++player)
      position[player] = std::clamp(position[player] + static_cast<int32_t>(buttons[player] & 1u) - static_cast<int32_t>(buttons[player] >> 1 & 1u), -10, 10);
    random = random * 1664525u + 1013904223u + buttons[0] * 31u + buttons[1];
    if (std::abs(position[0] - position[1]) <= 2) {
      if (buttons[0] & 4u)
        life[1] -= 1 + static_cast<int32_t>(random >> 28);
      if (buttons[1] & 4u)
        life[0] -= 1 + static_cast<int32_t>(random >> 28);
    }
    // Something that is not a function of the buttons, on one machine only, as a build that rounds
    // differently would be.
    if (tick == corruptAt)
      ++life[0];
    digest = (digest ^ random ^ static_cast<uint32_t>(life[0] * 3 + life[1] * 5 + position[0] * 7 + position[1])) * 16777619u;
    trace[tick] = digest;
  }
  auto Save(std::vector<std::byte> &out) const -> void override {
    StateWriter writer(out);
    for (auto player = 0; player < 2; ++player) {
      writer.Write(position[player]);
      writer.Write(life[player]);
    }
    writer.Write(random);
    writer.Write(digest);
  }
  auto Load(const std::span<const std::byte> in) -> void override {
    StateReader reader(in);
    for (auto player = 0; player < 2; ++player) {
      position[player] = reader.Read<int32_t>();
      life[player] = reader.Read<int32_t>();
    }
    random = reader.Read<uint32_t>();
    digest = reader.Read<uint32_t>();
    EXPECT_TRUE(reader.Ok());
  }
private:
  int64_t corruptAt;
};
/// What a player does, as a function of the tick they are asked on. Held for a few ticks at a time,
/// as fingers are, so that a guess that nothing has changed is right often but not always.
auto Script(const int player, const int64_t tick) -> ButtonMask {
  auto hash = static_cast<uint32_t>(tick / (4 + player * 3)) * 2654435761u + static_cast<uint32_t>(player) * 40503u;
  hash ^= hash >> 15;
  return hash >> 3 & 7u;
}
/// The game as it would have gone with no link at all, which is the answer a session must agree with.
auto Reference(const int64_t ticks, const int delay) -> ToyGame {
  ToyGame game;
  for (int64_t tick = 0; tick < ticks; ++tick) {
    // A button asked for on a tick counts `delay` ticks later, and the first ticks have none.
    const std::array<ButtonMask, 2> buttons{tick < delay ? 0u : Script(0, tick - delay), tick < delay ? 0u : Script(1, tick - delay)};
    game.Advance(tick, buttons, false);
  }
  return game;
}
struct Pair {
  Pair(const SimulatedLink::Conditions &conditions, const RollbackConfig &config = {}, const int64_t corruptSecond = -1)
    : link(conditions), firstGame(), secondGame(corruptSecond), first(firstGame, link.First(), With(config, 0)), second(secondGame, link.Second(), With(config, 1)) {}
  SimulatedLink link;
  ToyGame firstGame;
  ToyGame secondGame;
  RollbackSession first;
  RollbackSession second;
  static auto With(RollbackConfig config, const int player) -> RollbackConfig {
    config.localPlayer = player;
    return config;
  }
};
/// Steps both sessions, one call each per unit of time, until both have run `ticks` ticks.
/// `secondEvery` makes the second run on only some of the units, to model a slower machine.
auto Play(Pair &pair, const int64_t ticks, const int secondEvery = 1) -> void {
  for (auto unit = 0; unit < ticks * 50 && (pair.first.GetTick() < ticks || pair.second.GetTick() < ticks); ++unit) {
    if (pair.first.GetTick() < ticks)
      pair.first.Step(Script(0, pair.first.GetTick()));
    if (pair.second.GetTick() < ticks && unit % secondEvery == 0)
      pair.second.Step(Script(1, pair.second.GetTick()));
    pair.link.Step();
  }
}
auto ExpectSettledTicksMatch(const ToyGame &game, const RollbackSession &session, const ToyGame &reference) -> void {
  for (int64_t tick = 0; tick <= session.GetConfirmedTick(); ++tick) {
    ASSERT_TRUE(game.trace.contains(tick)) << "tick " << tick;
    ASSERT_EQ(game.trace.at(tick), reference.trace.at(tick)) << "tick " << tick << " came out differently from the game run with no link";
  }
}
void ExpectAgreesWithLocalRun(Pair &pair, const int64_t ticks, const int delay) {
  const auto reference = Reference(ticks, delay);
  EXPECT_EQ(pair.first.GetTick(), ticks);
  EXPECT_EQ(pair.second.GetTick(), ticks);
  EXPECT_GT(pair.first.GetConfirmedTick(), ticks / 2);
  EXPECT_GT(pair.second.GetConfirmedTick(), ticks / 2);
  ExpectSettledTicksMatch(pair.firstGame, pair.first, reference);
  ExpectSettledTicksMatch(pair.secondGame, pair.second, reference);
  EXPECT_FALSE(pair.first.IsDesynced());
  EXPECT_FALSE(pair.second.IsDesynced());
}
} // namespace
TEST(StateWriter, ReadsBackWhatWasWrittenInOrder) {
  std::vector<std::byte> bytes;
  StateWriter writer(bytes);
  writer.Write(int32_t{-7});
  writer.Write(2.5f);
  writer.Write(uint8_t{9});
  StateReader reader(bytes);
  EXPECT_EQ(reader.Read<int32_t>(), -7);
  EXPECT_FLOAT_EQ(reader.Read<float>(), 2.5f);
  EXPECT_EQ(reader.Read<uint8_t>(), 9);
  EXPECT_TRUE(reader.Ok());
}
TEST(StateWriter, ReadingPastTheEndGivesZeroAndSaysSo) {
  std::vector<std::byte> bytes;
  StateWriter(bytes).Write(uint8_t{1});
  StateReader reader(bytes);
  EXPECT_EQ(reader.Read<uint32_t>(), 0u);
  EXPECT_FALSE(reader.Ok());
}
TEST(SimulatedLink, DeliversAfterTheLatencyAndNotBefore) {
  SimulatedLink link({.latency = 3});
  const std::array<std::byte, 2> sent{std::byte{1}, std::byte{2}};
  link.First().Send(sent);
  std::vector<std::byte> received;
  for (auto step = 0; step < 3; ++step) {
    EXPECT_FALSE(link.Second().Receive(received)) << "step " << step;
    link.Step();
  }
  ASSERT_TRUE(link.Second().Receive(received));
  EXPECT_EQ(received, std::vector<std::byte>(sent.begin(), sent.end()));
  EXPECT_FALSE(link.First().Receive(received));
}
TEST(SimulatedLink, LosesAboutAsManyAsItIsTold) {
  SimulatedLink link({.loss = .3f, .seed = 7});
  const std::array<std::byte, 1> sent{std::byte{1}};
  for (auto count = 0; count < 2000; ++count)
    link.First().Send(sent);
  EXPECT_NEAR(link.GetDropped(), 600, 100);
}
TEST(SimulatedLink, FailsTheSameWayEveryTime) {
  auto drops = [] {
    SimulatedLink link({.loss = .5f, .seed = 3});
    const std::array<std::byte, 1> sent{std::byte{1}};
    for (auto count = 0; count < 100; ++count)
      link.First().Send(sent);
    return link.GetDropped();
  };
  EXPECT_EQ(drops(), drops());
}
TEST(RollbackSession, AgreesWithALocalRunOverAPerfectLink) {
  Pair pair({});
  Play(pair, 600);
  ExpectAgreesWithLocalRun(pair, 600, 2);
}
TEST(RollbackSession, AgreesWithALocalRunAcrossLatencyAndJitter) {
  Pair pair({.latency = 4, .jitter = 3, .seed = 11});
  Play(pair, 900);
  ExpectAgreesWithLocalRun(pair, 900, 2);
  // A guess has to have been wrong for this to say anything about rolling back.
  EXPECT_GT(pair.first.GetStats().rollbacks + pair.second.GetStats().rollbacks, 0);
}
TEST(RollbackSession, AgreesWithALocalRunWhenDatagramsAreLostAndRepeated) {
  Pair pair({.latency = 3, .jitter = 2, .loss = .25f, .duplicate = .15f, .seed = 5});
  Play(pair, 900);
  ExpectAgreesWithLocalRun(pair, 900, 2);
}
TEST(RollbackSession, AgreesWithALocalRunWithNoInputDelay) {
  Pair pair({.latency = 2, .jitter = 1, .seed = 2}, {.inputDelay = 0});
  Play(pair, 600);
  ExpectAgreesWithLocalRun(pair, 600, 0);
}
TEST(RollbackSession, NeverTakesBackATickWhenItMayNotGuess) {
  Pair pair({.latency = 3, .jitter = 2, .seed = 9}, {.maxPrediction = 0});
  Play(pair, 400);
  ExpectAgreesWithLocalRun(pair, 400, 2);
  EXPECT_EQ(pair.first.GetStats().rollbacks, 0);
  EXPECT_EQ(pair.second.GetStats().rollbacks, 0);
  EXPECT_GT(pair.first.GetStats().stalls, 0);
}
TEST(RollbackSession, WaitsRatherThanGuessFurtherThanItMay) {
  Pair pair({.latency = 12, .jitter = 4, .seed = 4});
  Play(pair, 500);
  ExpectAgreesWithLocalRun(pair, 500, 2);
  EXPECT_GT(pair.first.GetStats().stalls, 0);
  EXPECT_LE(pair.first.GetStats().deepest, pair.first.GetConfig().maxPrediction);
  EXPECT_LE(pair.second.GetStats().deepest, pair.second.GetConfig().maxPrediction);
}
TEST(RollbackSession, AdvancesTheGameOnlyOncePerTickWhenNothingIsTakenBack) {
  Pair pair({}, {.maxPrediction = 0});
  Play(pair, 100);
  EXPECT_EQ(pair.first.GetStats().resimulated, 0);
}
TEST(RollbackSession, FindsTheTickTheGamesStartedToDiffer) {
  Pair pair({.latency = 2, .seed = 1}, {.checksumInterval = 10}, 100);
  Play(pair, 300);
  EXPECT_TRUE(pair.first.IsDesynced());
  EXPECT_TRUE(pair.second.IsDesynced());
  // The state before tick 101 is the first to include the difference, and 110 the first multiple of
  // the interval after it.
  EXPECT_EQ(pair.first.GetDesyncTick(), 110);
  EXPECT_EQ(pair.second.GetDesyncTick(), 110);
}
TEST(RollbackSession, SaysWhichMachineIsAhead) {
  Pair pair({.latency = 2, .seed = 1});
  for (auto unit = 0; unit < 200; ++unit) {
    pair.first.Step(Script(0, pair.first.GetTick()));
    if (unit % 2 == 0)
      pair.second.Step(Script(1, pair.second.GetTick()));
    pair.link.Step();
  }
  EXPECT_GT(pair.first.GetAdvantage(), 0);
  EXPECT_LT(pair.second.GetAdvantage(), 0);
  // The one that runs every unit cannot get further ahead than it may guess.
  EXPECT_LE(pair.first.GetTick() - pair.second.GetTick(), pair.first.GetConfig().maxPrediction + pair.first.GetConfig().inputDelay + 2);
}
TEST(RollbackSession, GivesUpOnAPeerThatWentQuiet) {
  Pair pair({.latency = 1}, {.timeout = 30});
  Play(pair, 60);
  EXPECT_FALSE(pair.first.IsDisconnected());
  pair.link.SetConditions({.loss = 1.f});
  auto status = RollbackStatus::Ran;
  for (auto unit = 0; unit < 100; ++unit) {
    status = pair.first.Step(0).status;
    pair.link.Step();
  }
  EXPECT_EQ(status, RollbackStatus::Disconnected);
  EXPECT_TRUE(pair.first.IsDisconnected());
}
TEST(RollbackSession, IgnoresDatagramsThatAreNotItsOwn) {
  Pair pair({});
  const std::array<std::byte, 40> noise{};
  for (auto unit = 0; unit < 5; ++unit) {
    pair.link.Second().Send(noise);
    pair.first.Step(0);
    pair.link.Step();
  }
  EXPECT_FALSE(pair.first.IsDisconnected());
  EXPECT_EQ(pair.first.GetConfirmedTick(), -1);
}
