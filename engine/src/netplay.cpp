#include <algorithm>
#include <format>
#include <netplay.hpp>
#include <spdlog/spdlog.h>
using namespace kuki;
namespace {
/// A tick or two ahead is what a link that takes a moment looks like from either end; only more than
/// that is worth running slow for.
constexpr int AHEAD_NOISE = 1;
/// How much slower per tick ahead beyond that, and the slowest the clock is ever run: a game at
/// four fifths of its speed is plainly slow, and one at less is not worth catching up for.
constexpr double SLOWING = .03;
constexpr double SLOWEST = .8;
/// How many ticks between lines in the log saying how the match is going: five seconds at sixty.
constexpr int64_t LOG_EVERY = 300;
} // namespace
Netplay::Netplay(NetplayConfig configuration)
  : config(std::move(configuration)) {
  if (!config.options)
    return;
  const auto &options = *config.options;
  if (options.bot && !config.bot)
    spdlog::warn("[Netplay] --bot was given, but this game has nobody to play for it");
  phase = Phase::Connecting;
  udp = options.host ? UdpTransport::Create(options.port) : UdpTransport::Create(0, options.address, options.port);
  if (!udp) {
    Fail("Could not open the connection");
    return;
  }
  Transport *wire = udp.get();
  if (options.lag > 0. || options.jitter > 0. || options.loss > 0.f) {
    lagged = std::make_unique<LaggedTransport>(*udp, LaggedTransport::Conditions{.latency = options.lag, .jitter = options.jitter, .loss = options.loss});
    wire = lagged.get();
    spdlog::info("[Netplay] Simulating {:.0f} ms each way, {:.0f} ms of jitter, {:.0f}% lost", options.lag * 1000., options.jitter * 1000., options.loss * 100.);
  }
  RollbackHandshake::Options handshakeOptions;
  handshakeOptions.protocol = config.protocol;
  handshakeOptions.tickRate = config.tickRate;
  handshakeOptions.config.inputDelay = options.inputDelay;
  handshakeOptions.config.maxPrediction = options.maxPrediction;
  handshake = std::make_unique<RollbackHandshake>(*wire, options.host ? RollbackHandshake::Role::Host : RollbackHandshake::Role::Join, handshakeOptions);
  if (options.host)
    spdlog::info("[Netplay] Waiting for the other player on port {}", udp->GetLocalPort());
  else
    spdlog::info("[Netplay] Calling {}:{}", options.address, options.port);
}
auto Netplay::Fail(std::string reason) -> void {
  spdlog::error("[Netplay] {}", reason);
  failure = std::move(reason);
  phase = Phase::Failed;
  scale = 1.;
}
auto Netplay::GetPhase() const -> Phase {
  return phase;
}
auto Netplay::IsOnline() const -> bool {
  return config.options.has_value();
}
auto Netplay::GetTickScale() const -> double {
  return scale;
}
auto Netplay::GetPort() const -> uint16_t {
  return udp ? udp->GetLocalPort() : 0;
}
auto Netplay::GetLocalPlayer() const -> int {
  return localPlayer;
}
auto Netplay::GetTick() const -> int64_t {
  return session ? session->GetTick() : aloneTick;
}
auto Netplay::GetSession() const -> const RollbackSession * {
  return session.get();
}
auto Netplay::GetRules() const -> const std::shared_ptr<RollbackGame> & {
  return config.rules;
}
auto Netplay::Seat(const int player, const ButtonMask local, const int64_t tick) const -> ButtonMask {
  if (config.seats[player])
    return config.seats[player](tick);
  return player == 0 ? local : 0u;
}
auto Netplay::Step(ButtonMask local, const double seconds) -> void {
  if (phase == Phase::Failed || !config.rules)
    return;
  if (!config.options) {
    const std::array<ButtonMask, 2> buttons{Seat(0, local, aloneTick), Seat(1, local, aloneTick)};
    config.rules->Advance(aloneTick, buttons, false);
    ++aloneTick;
    return;
  }
  if (phase == Phase::Connecting) {
    switch (handshake->Poll(seconds)) {
    case RollbackHandshake::Status::Waiting:
      return;
    case RollbackHandshake::Status::Failed:
      Fail(handshake->GetFailure());
      return;
    case RollbackHandshake::Status::Ready:
      break;
    }
    const auto &result = handshake->GetResult();
    if (result.tickRate != config.tickRate) {
      Fail(std::format("The host plays at {} ticks a second and this plays at {}", result.tickRate, config.tickRate));
      return;
    }
    localPlayer = result.localPlayer;
    Transport &wire = lagged ? static_cast<Transport &>(*lagged) : static_cast<Transport &>(*udp);
    session = std::make_unique<RollbackSession>(*config.rules, wire, result.config);
    phase = Phase::Playing;
    spdlog::info("[Netplay] Connected as player {}, input delay {}, guessing up to {} ticks ahead", localPlayer + 1, result.config.inputDelay, result.config.maxPrediction);
  }
  if (config.options->bot && config.bot)
    local = config.bot(localPlayer, session->GetTick());
  if (session->Step(local).status == RollbackStatus::Disconnected) {
    Fail("The other player stopped responding");
    return;
  }
  // Said in the log every few seconds as well as on the screen, for a run nobody is watching.
  if (const auto period = session->GetTick() / LOG_EVERY; period > logged) {
    logged = period;
    for (const auto &line : GetStatus())
      spdlog::info("[Netplay] {}", line);
  }
  const auto ahead = session->GetAdvantage() - AHEAD_NOISE;
  scale = ahead > 0 ? std::max(SLOWEST, 1. - SLOWING * ahead) : 1.;
}
auto Netplay::GetStatus() const -> std::vector<std::string> {
  std::vector<std::string> lines;
  if (!config.options)
    return lines;
  const auto &options = *config.options;
  if (phase == Phase::Failed) {
    lines.push_back(failure);
    return lines;
  }
  if (phase == Phase::Connecting) {
    lines.push_back(options.host ? std::format("Waiting for the other player on port {}", GetPort()) : std::format("Calling {}:{}", options.address, options.port));
    return lines;
  }
  const auto &stats = session->GetStats();
  lines.push_back(std::format("Player {} of 2, tick {}, settled to {}", localPlayer + 1, session->GetTick(), session->GetConfirmedTick()));
  lines.push_back(std::format("Took back {} times, {} ticks at most; waited {} times; ahead by {}; checked {} times", stats.rollbacks, stats.deepest, stats.stalls, session->GetAdvantage(), stats.checksumsCompared));
  if (options.lag > 0. || options.jitter > 0. || options.loss > 0.f)
    lines.push_back(std::format("Simulating {:.0f} ms each way, {:.0f}% lost", options.lag * 1000., options.loss * 100.));
  if (session->IsDesynced())
    lines.push_back(std::format("THE GAMES HAVE DIVERGED at tick {}", session->GetDesyncTick()));
  return lines;
}
