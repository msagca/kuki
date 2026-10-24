#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <gesture.hpp>
#include <kuki_engine_export.h>
#include <memory>
#include <netplay.hpp>
#include <network_options.hpp>
#include <optional>
#include <rollback.hpp>
#include <system.hpp>
namespace kuki {
/// @brief Plays a game's rules against another machine, or alone, a tick at a time.
///
/// An application lists it ahead of the game's own systems, and a game hands it the rules with
/// `Begin`. From then on the rules are advanced by this and not by the game: once a tick when
/// playing alone, and when playing over a network as often as the session needs, which includes
/// running a tick again that was run on a wrong guess. That is why the rules are a `RollbackGame`
/// and not a system's `Tick`, and why the game's systems are for what is drawn. They run after this
/// each tick and find the rules where it has left them.
///
/// Command-line arguments choose how the match is played, so that every game gets them without
/// asking: see `NetworkOptions`. Nothing given, it is played alone and this does no more than step
/// the rules. An application without this system in its list has no `Application::GetNetwork`, and
/// a game that asks for it and finds none steps its own rules, which is what an editor hosting the
/// game does.
///
/// It also slows the tick clock a little when this machine is ahead of the other, and writes how the
/// match is going in the corner of the screen while one is under way over a network. Both want the
/// overlay to have a font, which a game sets itself.
class KUKI_ENGINE_API NetworkSystem final : public System {
public:
  using Seat = std::function<ButtonMask(const int64_t)>;
  using Bot = std::function<ButtonMask(const int, const int64_t)>;
  /// The flags it reads. See `NetworkOptions`.
  static constexpr auto FLAGS = NETWORK_FLAGS;
  explicit NetworkSystem(Application &);
  /// @brief Makes itself known to the application and reads the command line.
  void Start() override;
  auto Tick() -> void override;
  void Update(const float) override;
  void Shutdown() override;
  /// @brief A number for the game and its version, which two machines must share to play. Set it
  /// before `Begin`, and change it whenever a change to the rules would make an old build and a new
  /// one play different games.
  auto SetProtocol(const uint32_t) -> void;
  /// @brief Who plays a seat when nobody is at it: given the tick, its buttons.
  ///
  /// For the other player of a game played alone. Seat 0 is the local player's unless a function is
  /// given for it, which is how a demonstration plays itself.
  auto SetSeat(const int, Seat) -> void;
  /// @brief What `--bot` plays from, given the seat and the tick.
  ///
  /// Over a network it plays for the local player. Alone, `--bot` plays the first seat, so that a
  /// game plays itself, which is how to watch one with nobody at the keyboard.
  auto SetBot(Bot) -> void;
  /// @brief Replaces what the command line said, or says it if the command line said nothing. Empty
  /// is a match alone. Takes effect at the next `Begin`.
  auto SetOptions(std::optional<NetworkOptions>) -> void;
  /// @brief Whether the corner of the screen says how the match is going. On.
  auto ShowStatus(const bool) -> void;
  /// @brief Starts a match on these rules, ending any that was under way.
  ///
  /// Over a network this opens the connection, which then takes as many ticks as it takes for the
  /// other player to turn up. The rules are held until `End`.
  auto Begin(std::shared_ptr<RollbackGame>) -> void;
  /// @brief Ends the match and lets go of the rules and the connection.
  auto End() -> void;
  /// @brief The match in progress, or null.
  auto GetNetplay() const -> const Netplay *;
  /// @brief Whether a match is under way, which is not whether it has begun: over a network it may
  /// still be waiting for the other player.
  auto IsActive() const -> bool;
private:
  std::optional<NetworkOptions> options;
  std::unique_ptr<Netplay> netplay;
  uint32_t protocol{1};
  std::array<Seat, 2> seats;
  Bot bot;
  bool showStatus{true};
  /// Whether the command line asked for `--bot` with no match over a network to apply it to.
  bool botAlone{};
};
} // namespace kuki
