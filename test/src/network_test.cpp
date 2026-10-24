#include <chrono>
#include <duel_sim.hpp>
#include <gtest/gtest.h>
#include <lagged_transport.hpp>
#include <model_asset.hpp>
#include <netplay.hpp>
#include <network_options.hpp>
#include <rollback_handshake.hpp>
#include <simulated_link.hpp>
#include <thread>
#include <udp_transport.hpp>
using namespace kuki;
namespace {
using Bytes = std::vector<std::byte>;
auto Make(const std::initializer_list<int> values) -> Bytes {
  Bytes bytes;
  for (const auto value : values)
    bytes.push_back(static_cast<std::byte>(value));
  return bytes;
}
/// Waits a moment for a datagram on a real socket, which on one machine is a very short one.
auto Await(Transport &transport, Bytes &out) -> bool {
  for (auto attempt = 0; attempt < 200; ++attempt) {
    if (transport.Receive(out))
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}
} // namespace
TEST(UdpTransport, CarriesDatagramsBothWaysAndLearnsTheCaller) {
  auto host = UdpTransport::Create(0);
  ASSERT_TRUE(host);
  EXPECT_FALSE(host->HasPeer());
  EXPECT_NE(host->GetLocalPort(), 0);
  auto joiner = UdpTransport::Create(0, "127.0.0.1", host->GetLocalPort());
  ASSERT_TRUE(joiner);
  Bytes received;
  EXPECT_FALSE(host->Receive(received));
  joiner->Send(Make({1, 2, 3}));
  ASSERT_TRUE(Await(*host, received));
  EXPECT_EQ(received, Make({1, 2, 3}));
  EXPECT_TRUE(host->HasPeer());
  host->Send(Make({9}));
  ASSERT_TRUE(Await(*joiner, received));
  EXPECT_EQ(received, Make({9}));
}
TEST(UdpTransport, IgnoresAnyoneElseOnceItHasAPeer) {
  auto host = UdpTransport::Create(0);
  ASSERT_TRUE(host);
  auto joiner = UdpTransport::Create(0, "127.0.0.1", host->GetLocalPort());
  auto stranger = UdpTransport::Create(0, "127.0.0.1", host->GetLocalPort());
  ASSERT_TRUE(joiner && stranger);
  Bytes received;
  joiner->Send(Make({1}));
  ASSERT_TRUE(Await(*host, received));
  stranger->Send(Make({2}));
  joiner->Send(Make({3}));
  ASSERT_TRUE(Await(*host, received));
  EXPECT_EQ(received, Make({3}));
  EXPECT_FALSE(host->Receive(received));
}
TEST(UdpTransport, SendingToNobodyIsHarmlessAndReadingNothingIsFalse) {
  auto lonely = UdpTransport::Create(0);
  ASSERT_TRUE(lonely);
  lonely->Send(Make({1}));
  Bytes received;
  EXPECT_FALSE(lonely->Receive(received));
}
TEST(UdpTransport, SurvivesAPeerThatIsNotListeningYet) {
  // A datagram to a port with nothing behind it is answered by the system with an error that some
  // platforms hand to the next read. It must not look like a failure to the one that sent it.
  auto joiner = UdpTransport::Create(0, "127.0.0.1", 9);
  ASSERT_TRUE(joiner);
  Bytes received;
  for (auto attempt = 0; attempt < 5; ++attempt) {
    joiner->Send(Make({1}));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_FALSE(joiner->Receive(received));
  }
}
TEST(LaggedTransport, HoldsDatagramsBackUntilTheirTime) {
  SimulatedLink link;
  double now = 0.;
  LaggedTransport lagged(link.First(), {.latency = .1}, [&] { return now; });
  lagged.Send(Make({1}));
  Bytes received;
  EXPECT_FALSE(link.Second().Receive(received));
  now = .05;
  lagged.Receive(received);
  EXPECT_FALSE(link.Second().Receive(received));
  now = .11;
  lagged.Receive(received);
  ASSERT_TRUE(link.Second().Receive(received));
  EXPECT_EQ(received, Make({1}));
}
TEST(LaggedTransport, LetsLaterDatagramsOvertakeEarlierOnes) {
  SimulatedLink link;
  double now = 0.;
  LaggedTransport lagged(link.First(), {.latency = .01, .jitter = .5, .seed = 5}, [&] { return now; });
  for (auto value = 0; value < 20; ++value)
    lagged.Send(Make({value}));
  now = 10.;
  Bytes received;
  lagged.Receive(received);
  std::vector<int> order;
  while (link.Second().Receive(received))
    order.push_back(std::to_integer<int>(received[0]));
  ASSERT_EQ(order.size(), 20u);
  EXPECT_FALSE(std::ranges::is_sorted(order));
}
TEST(LaggedTransport, LosesAboutAsManyAsItIsTold) {
  SimulatedLink link;
  double now = 0.;
  LaggedTransport lagged(link.First(), {.loss = .4f, .seed = 3}, [&] { return now; });
  for (auto count = 0; count < 1000; ++count)
    lagged.Send(Make({1}));
  Bytes received;
  auto arrived = 0;
  while (link.Second().Receive(received))
    ++arrived;
  EXPECT_NEAR(arrived, 600, 80);
}
TEST(RollbackHandshake, SettlesPlayersAndTakesTheHostsSettingsAcrossALossyLink) {
  SimulatedLink link({.latency = 2, .jitter = 2, .loss = .4f, .duplicate = .2f, .seed = 8});
  RollbackHandshake::Options hostOptions;
  hostOptions.tickRate = 120;
  hostOptions.config.inputDelay = 3;
  hostOptions.config.maxPrediction = 6;
  hostOptions.config.checksumInterval = 20;
  RollbackHandshake::Options joinOptions;
  joinOptions.config.inputDelay = 9;
  RollbackHandshake host(link.First(), RollbackHandshake::Role::Host, hostOptions);
  RollbackHandshake joiner(link.Second(), RollbackHandshake::Role::Join, joinOptions);
  for (auto step = 0; step < 2000 && (host.GetStatus() == RollbackHandshake::Status::Waiting || joiner.GetStatus() == RollbackHandshake::Status::Waiting); ++step) {
    host.Poll(.016);
    joiner.Poll(.016);
    link.Step();
  }
  ASSERT_EQ(host.GetStatus(), RollbackHandshake::Status::Ready);
  ASSERT_EQ(joiner.GetStatus(), RollbackHandshake::Status::Ready);
  EXPECT_EQ(host.GetResult().localPlayer, 0);
  EXPECT_EQ(joiner.GetResult().localPlayer, 1);
  EXPECT_EQ(host.GetResult().config.localPlayer, 0);
  EXPECT_EQ(joiner.GetResult().config.localPlayer, 1);
  EXPECT_EQ(joiner.GetResult().tickRate, 120);
  EXPECT_EQ(joiner.GetResult().config.inputDelay, 3);
  EXPECT_EQ(joiner.GetResult().config.maxPrediction, 6);
  EXPECT_EQ(joiner.GetResult().config.checksumInterval, 20);
}
TEST(RollbackHandshake, TurnsAwayAHostOfAnotherVersion) {
  SimulatedLink link;
  RollbackHandshake::Options hostOptions;
  hostOptions.protocol = 2;
  RollbackHandshake host(link.First(), RollbackHandshake::Role::Host, hostOptions);
  RollbackHandshake joiner(link.Second(), RollbackHandshake::Role::Join, {});
  for (auto step = 0; step < 50 && joiner.GetStatus() == RollbackHandshake::Status::Waiting; ++step) {
    host.Poll(.016);
    joiner.Poll(.016);
    link.Step();
  }
  EXPECT_EQ(joiner.GetStatus(), RollbackHandshake::Status::Failed);
  EXPECT_EQ(host.GetStatus(), RollbackHandshake::Status::Waiting);
}
TEST(RollbackHandshake, GivesUpOnAHostThatNeverAnswers) {
  SimulatedLink link;
  RollbackHandshake::Options options;
  options.timeout = 1.;
  RollbackHandshake joiner(link.Second(), RollbackHandshake::Role::Join, options);
  for (auto step = 0; step < 100; ++step) {
    joiner.Poll(.1);
    link.Step();
  }
  EXPECT_EQ(joiner.GetStatus(), RollbackHandshake::Status::Failed);
  EXPECT_FALSE(joiner.GetFailure().empty());
}
TEST(RollbackHandshake, ALostReadyIsMadeUpByTheFirstDatagramOfTheSession) {
  SimulatedLink link;
  RollbackHandshake host(link.First(), RollbackHandshake::Role::Host, {});
  RollbackHandshake joiner(link.Second(), RollbackHandshake::Role::Join, {});
  joiner.Poll(.1);
  host.Poll(.1);
  joiner.Poll(.1);
  ASSERT_EQ(joiner.GetStatus(), RollbackHandshake::Status::Ready);
  // The three Readys never arrive; what does is the session the joiner went on to start.
  Bytes discarded;
  while (link.First().Receive(discarded)) {
  }
  struct Nothing final : RollbackGame {
    auto Advance(const int64_t, const std::span<const ButtonMask>, const bool) -> void override {}
    auto Save(std::vector<std::byte> &out) const -> void override {
      out.push_back(std::byte{});
    }
    auto Load(const std::span<const std::byte>) -> void override {}
  } game;
  host.Poll(.01);
  EXPECT_EQ(host.GetStatus(), RollbackHandshake::Status::Waiting);
  RollbackSession session(game, link.Second(), joiner.GetResult().config);
  session.Step(0);
  host.Poll(.01);
  EXPECT_EQ(host.GetStatus(), RollbackHandshake::Status::Ready);
}
TEST(NetworkOptions, ReadsAHostAndAJoinerAndTheirConditions) {
  std::string error;
  const std::vector<std::string> host{"--host", "7000", "--lag", "80", "--jitter", "20", "--loss", "5", "--delay", "3", "--prediction", "6", "--bot"};
  const auto parsed = ParseNetworkOptions(host, error);
  ASSERT_TRUE(parsed) << error;
  EXPECT_TRUE(parsed->host);
  EXPECT_EQ(parsed->port, 7000);
  EXPECT_DOUBLE_EQ(parsed->lag, .08);
  EXPECT_DOUBLE_EQ(parsed->jitter, .02);
  EXPECT_FLOAT_EQ(parsed->loss, .05f);
  EXPECT_EQ(parsed->inputDelay, 3);
  EXPECT_EQ(parsed->maxPrediction, 6);
  EXPECT_TRUE(parsed->bot);
  const std::vector<std::string> join{"--join", "192.168.1.5:7001"};
  const auto joined = ParseNetworkOptions(join, error);
  ASSERT_TRUE(joined) << error;
  EXPECT_FALSE(joined->host);
  EXPECT_EQ(joined->address, "192.168.1.5");
  EXPECT_EQ(joined->port, 7001);
}
TEST(NetworkOptions, IsOfflineWithoutHostOrJoinAndSaysWhenItIsMistaken) {
  std::string error;
  EXPECT_FALSE(ParseNetworkOptions(std::vector<std::string>{"--bot", "--lag", "50", "--game", "duel"}, error));
  EXPECT_TRUE(error.empty());
  EXPECT_FALSE(ParseNetworkOptions(std::vector<std::string>{"--host", "7000", "--join", "x:1"}, error));
  EXPECT_FALSE(error.empty());
  error.clear();
  EXPECT_FALSE(ParseNetworkOptions(std::vector<std::string>{"--join", "nowhere"}, error));
  EXPECT_FALSE(error.empty());
  error.clear();
  EXPECT_FALSE(ParseNetworkOptions(std::vector<std::string>{"--host"}, error));
  EXPECT_FALSE(error.empty());
}
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
}
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
