#pragma once
#include <kuki_engine_export.h>
#include <rollback.hpp>
#include <string>
#include <transport.hpp>
#include <vector>
namespace kuki {
/// @brief What the two machines settled before the first tick.
struct HandshakeResult {
  /// 0 for the one that hosted, 1 for the one that joined.
  int localPlayer{};
  /// The host's settings, with `localPlayer` filled in for this machine.
  RollbackConfig config;
  int tickRate{60};
};
/// @brief Brings two machines to the point of starting a `RollbackSession` together.
///
/// A session assumes that both ends begin on the same tick, with the same rules, and know which
/// player each is. None of that can be assumed of two programs started by hand, and a mistake in any
/// of it does not fail: the games just quietly differ. So this settles it first, over the same
/// transport the session will use afterwards.
///
/// One end hosts and waits; the other joins and keeps calling out. The joiner sends `Hello`; the
/// host answers with `Welcome`, which carries the host's settings, because an input delay or a tick
/// rate that differs between the two is not a setting but a desync; the joiner sends `Ready` and
/// starts, and the host starts when that, or the first datagram of the session, reaches it. Every
/// step is repeated until it is answered, so a lost datagram delays the start and does not stop it.
///
/// The two start a trip's time apart, one when it has the other's word and the other when it has
/// heard that it was received. The session does not mind: a machine that is ahead by that much
/// waits for the other, and `RollbackSession::GetAdvantage` says how much.
///
/// Both ends must give the same `protocol`. It is a number for the game and its version to put in,
/// so that a copy of a different build is turned away instead of being played against.
class KUKI_ENGINE_API RollbackHandshake {
public:
  enum class Role { Host,
    Join };
  enum class Status { Waiting,
    Ready,
    Failed };
  struct Options {
    uint32_t protocol{1};
    /// The settings the match will use. Only the host's are used: the joiner takes them from it.
    int tickRate{60};
    RollbackConfig config;
    /// How long to try, in seconds. A host waits as long as it takes for somebody to call; this
    /// starts counting from the first call.
    double timeout{30.};
    /// How often to say it again, in seconds, until it is answered.
    double resend{.1};
  };
  /// The transport must outlive this, and is left for the session once it is `Ready`.
  RollbackHandshake(Transport &, const Role, const Options &);
  /// @brief Reads what has arrived, answers it, and says again what has gone unanswered.
  ///
  /// `seconds` is how long since the last call. Once not `Waiting` it does nothing.
  auto Poll(const double seconds) -> Status;
  auto GetStatus() const -> Status;
  /// @brief What was settled. Only meaningful once `Ready`.
  auto GetResult() const -> const HandshakeResult &;
  /// @brief Why it failed, once `Failed`.
  auto GetFailure() const -> const std::string &;
private:
  Transport &transport;
  Role role;
  Options options;
  Status status{Status::Waiting};
  HandshakeResult result;
  std::string failure;
  std::vector<std::byte> inbox;
  std::vector<std::byte> outbox;
  double sinceSent{};
  double waited{};
  bool heard{};
  bool sentYet{};
  auto Handle() -> void;
  auto SendHello() -> void;
  auto SendWelcome() -> void;
  auto SendReady() -> void;
  auto SendReject() -> void;
  auto Fail(std::string) -> void;
};
} // namespace kuki
