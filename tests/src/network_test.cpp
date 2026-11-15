#include <chrono>
#include <gtest/gtest.h>
#include <lagged_transport.hpp>
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
