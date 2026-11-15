#include <chrono>
#include <duel_sim.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <model_asset.hpp>
#include <netplay.hpp>
#include <network_options.hpp>
#include <optional>
#include <thread>
// The engine's `Netplay` with the duel's rules as what it plays: alone, over a real socket with lag
// and loss, and against a host of another version. Here rather than beside the engine's transport
// tests because it needs a game to play, and the duel is the one played over the network.
using namespace kuki;
namespace {
auto Clip(std::string name, const float seconds) -> AnimationClip {
  AnimationClip clip;
  clip.name = std::move(name);
  clip.duration = seconds * 30.f;
  clip.ticksPerSecond = 30.f;
  return clip;
}
auto MakeModel() -> ModelAsset {
  ModelAsset model{AssetID{}};
  model.nodes.resize(2);
  model.nodes[0].name = "Fighter";
  model.nodes[0].transform = glm::mat4(1.f);
  model.nodes[1].name = "Root";
  model.nodes[1].parent = 0;
  model.nodes[1].transform = glm::mat4(1.f);
  auto walk = Clip("Walk", 1.f);
  walk.channels.push_back({.nodeIndex = 1, .nodeName = "Root", .positions = {{.0f, {.0f, .0f, .0f}}, {30.f, {.0f, .0f, 2.f}}}});
  model.animations = {Clip("Idle", 1.f), walk, Clip("Punch", 1.f), Clip("Kick", 1.f), Clip("Hit", .5f)};
  return model;
}
/// A match of the duel rules for one end, with the unattended seat played by the rules themselves.
auto MakeConfig(const ModelAsset &model, std::optional<NetworkOptions> options) -> NetplayConfig {
  auto rules = std::make_shared<duel::DuelSim>(model, 60);
  rules->SetLogging(false);
  NetplayConfig config;
  config.options = std::move(options);
  config.rules = rules;
  config.protocol = duel::PROTOCOL;
  config.seats[1] = [rules](const int64_t) { return rules->Think(1); };
  config.bot = duel::ScriptedButtons;
  return config;
}
auto Sim(const Netplay &netplay) -> const duel::DuelSim & {
  return static_cast<const duel::DuelSim &>(*netplay.GetRules());
}
} // namespace
TEST(Netplay, PlaysAloneByAdvancingTheRulesOncePerStep) {
  const auto model = MakeModel();
  Netplay alone(MakeConfig(model, std::nullopt));
  EXPECT_FALSE(alone.IsOnline());
  EXPECT_EQ(alone.GetPhase(), Netplay::Phase::Playing);
  const auto before = Sim(alone).GetFighter(0).position;
  for (auto step = 0; step < 90; ++step)
    alone.Step(1u << duel::Right, .016);
  EXPECT_EQ(alone.GetTick(), 90);
  EXPECT_EQ(Sim(alone).GetTick(), 90);
  EXPECT_GT(Sim(alone).GetFighter(0).position, before);
  EXPECT_TRUE(alone.GetStatus().empty());
  EXPECT_EQ(alone.GetTickScale(), 1.);
}
TEST(Netplay, TheSeatsPressWhatTheyAreGiven) {
  const auto model = MakeModel();
  auto config = MakeConfig(model, std::nullopt);
  // Nobody at the keyboard, and the second seat standing still: the first plays itself.
  config.seats[0] = [](const int64_t) { return 1u << duel::Right; };
  config.seats[1] = {};
  Netplay alone(std::move(config));
  for (auto step = 0; step < 60; ++step)
    alone.Step(0, .016);
  EXPECT_GT(Sim(alone).GetFighter(0).position, -1.6f);
  EXPECT_FLOAT_EQ(Sim(alone).GetFighter(1).position, 1.6f);
}
TEST(Netplay, TwoCopiesOfTheGameFindEachOtherOverARealSocketAndStayInStep) {
  const auto model = MakeModel();
  // A little lag each way, and some loss, on top of a loopback that has none: enough that guesses
  // about the other's buttons are wrong and have to be taken back.
  NetworkOptions hostOptions{.host = true, .lag = .005, .jitter = .004, .loss = .05f, .bot = true};
  Netplay host(MakeConfig(model, hostOptions));
  ASSERT_NE(host.GetPort(), 0);
  NetworkOptions joinOptions{.port = host.GetPort(), .address = "127.0.0.1", .lag = .005, .jitter = .004, .loss = .05f, .bot = true};
  Netplay joiner(MakeConfig(model, joinOptions));
  constexpr int64_t TICKS = 600;
  const auto ticks = [](const Netplay &netplay) { return netplay.GetSession() ? netplay.GetSession()->GetTick() : int64_t{0}; };
  for (auto iteration = 0; iteration < 20000 && (ticks(host) < TICKS || ticks(joiner) < TICKS); ++iteration) {
    host.Step(0, .002);
    joiner.Step(0, .002);
    // Not a sleep: the system's shortest is several times longer than the delay being simulated.
    std::this_thread::yield();
  }
  ASSERT_EQ(host.GetPhase(), Netplay::Phase::Playing);
  ASSERT_EQ(joiner.GetPhase(), Netplay::Phase::Playing);
  EXPECT_EQ(host.GetLocalPlayer(), 0);
  EXPECT_EQ(joiner.GetLocalPlayer(), 1);
  EXPECT_GE(ticks(host), TICKS);
  EXPECT_GE(ticks(joiner), TICKS);
  const auto &hostSession = *host.GetSession();
  const auto &joinSession = *joiner.GetSession();
  EXPECT_FALSE(hostSession.IsDesynced());
  EXPECT_FALSE(joinSession.IsDesynced());
  // Nothing has been shown to agree unless the two have actually compared notes.
  EXPECT_GT(hostSession.GetStats().checksumsCompared, 5);
  EXPECT_GT(joinSession.GetStats().checksumsCompared, 5);
  EXPECT_GT(hostSession.GetStats().rollbacks + joinSession.GetStats().rollbacks, 0);
  EXPECT_GT(Sim(host).GetFighter(0).hits + Sim(host).GetFighter(1).hits + Sim(joiner).GetFighter(0).hits + Sim(joiner).GetFighter(1).hits, 0);
}
TEST(Netplay, ReportsWhenNobodyAnswers) {
  const auto model = MakeModel();
  Netplay joiner(MakeConfig(model, NetworkOptions{.port = 9, .address = "127.0.0.1"}));
  for (auto step = 0; step < 4000 && joiner.GetPhase() == Netplay::Phase::Connecting; ++step)
    joiner.Step(0, .01);
  EXPECT_EQ(joiner.GetPhase(), Netplay::Phase::Failed);
  EXPECT_FALSE(joiner.GetStatus().empty());
}
TEST(Netplay, TurnsAwayAHostOfAnotherVersion) {
  const auto model = MakeModel();
  Netplay host(MakeConfig(model, NetworkOptions{.host = true}));
  auto config = MakeConfig(model, NetworkOptions{.port = host.GetPort(), .address = "127.0.0.1"});
  config.protocol = duel::PROTOCOL + 1;
  Netplay joiner(std::move(config));
  for (auto step = 0; step < 400 && joiner.GetPhase() == Netplay::Phase::Connecting; ++step) {
    host.Step(0, .01);
    joiner.Step(0, .01);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_EQ(joiner.GetPhase(), Netplay::Phase::Failed);
  EXPECT_EQ(host.GetPhase(), Netplay::Phase::Connecting);
}
