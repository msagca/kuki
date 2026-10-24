#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <gesture.hpp>
#include <kuki_engine_export.h>
#include <lagged_transport.hpp>
#include <memory>
#include <network_options.hpp>
#include <optional>
#include <rollback.hpp>
#include <rollback_handshake.hpp>
#include <string>
#include <udp_transport.hpp>
#include <vector>
namespace kuki {
/// @brief What a match needs to know that its rules do not say.
struct NetplayConfig {
  /// Absent for a match played alone on this machine.
  std::optional<NetworkOptions> options;
  /// The game. Held for as long as the match is, since a session keeps a reference to it.
  std::shared_ptr<RollbackGame> rules;
  int tickRate{60};
  /// A number for the game and its version. Two machines that give different ones are not let play.
  uint32_t protocol{1};
  /// Who sits in a seat when the match is played alone, or in a seat nobody is at: given the tick,
  /// the buttons. Seat 0 is the local player's unless a function is given for it. Empty is a player
  /// who presses nothing.
  std::array<std::function<ButtonMask(const int64_t)>, 2> seats;
  /// What `--bot` plays from, given the seat and the tick.
  std::function<ButtonMask(const int, const int64_t)> bot;
};
/// @brief A match from start to finish, played alone or over a network.
///
/// Alone, it is the rules advanced once a tick with whatever the seats press. Over a network it
/// opens the socket, goes through the handshake and then runs a `RollbackSession` on the rules, and
/// the rules cannot tell the difference: they are given the buttons of both seats for a tick either
/// way. That is the point of having it, and it is why a networked game keeps its rules in a
/// `RollbackGame` and not in a system's `Tick`, which a rollback has no way to run again.
///
/// Knows nothing of a window or an application, so it can be run from a test. `NetworkSystem` is
/// what puts it in a game.
class KUKI_ENGINE_API Netplay {
public:
  enum class Phase { Connecting,
    Playing,
    Failed };
  explicit Netplay(NetplayConfig);
  /// @brief Does what a tick calls for: while connecting, the handshake; after, one step of the match.
  ///
  /// `local` is what the local player pressed and `seconds` the length of a tick, for the handshake
  /// to count its waiting by.
  auto Step(const ButtonMask local, const double seconds) -> void;
  auto GetPhase() const -> Phase;
  auto IsOnline() const -> bool;
  /// @brief How fast the tick clock should run, from the session's idea of how far ahead it is.
  ///
  /// One until the machine has got ahead by more than the usual noise, and then a little less, in
  /// proportion, so that the other catches up by this one slowing and not by this one stopping.
  auto GetTickScale() const -> double;
  /// @brief The port the socket is on, which for a host started with port zero is the one it was given.
  auto GetPort() const -> uint16_t;
  /// @brief Which seat is the local player's. Always 0 when playing alone.
  auto GetLocalPlayer() const -> int;
  /// @brief The number of the tick the next step will run.
  auto GetTick() const -> int64_t;
  auto GetSession() const -> const RollbackSession *;
  auto GetRules() const -> const std::shared_ptr<RollbackGame> &;
  /// @brief What to tell whoever is playing about how it is going, a line at a time. Nothing when
  /// playing alone and nothing is wrong.
  auto GetStatus() const -> std::vector<std::string>;
private:
  NetplayConfig config;
  Phase phase{Phase::Playing};
  std::string failure;
  double scale{1.};
  std::unique_ptr<UdpTransport> udp;
  std::unique_ptr<LaggedTransport> lagged;
  std::unique_ptr<RollbackHandshake> handshake;
  std::unique_ptr<RollbackSession> session;
  int localPlayer{};
  int64_t aloneTick{};
  int64_t logged{};
  auto Fail(std::string) -> void;
  auto Seat(const int, const ButtonMask, const int64_t) const -> ButtonMask;
};
} // namespace kuki
