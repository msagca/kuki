#pragma once
#include <array>
#include <command_line.hpp>
#include <cstdint>
#include <kuki_engine_export.h>
#include <optional>
#include <span>
#include <string>
namespace kuki {
inline constexpr Flag HOST_FLAG{"--host", FlagKind::Value, "<port>", "wait for the other player to call on this port"};
inline constexpr Flag JOIN_FLAG{"--join", FlagKind::Value, "<host:port>", "call the host"};
inline constexpr Flag LAG_FLAG{"--lag", FlagKind::Value, "<ms>", "hold back what is sent by this long, so one PC can stand in for the internet"};
inline constexpr Flag JITTER_FLAG{"--jitter", FlagKind::Value, "<ms>", "and by up to this much more, which also reorders it"};
inline constexpr Flag LOSS_FLAG{"--loss", FlagKind::Value, "<percent>", "and lose this share of it"};
inline constexpr Flag BOT_FLAG{"--bot", FlagKind::Switch, "", "play from the game's own script of buttons instead of the keyboard"};
inline constexpr Flag DELAY_FLAG{"--delay", FlagKind::Value, "<ticks>", "input delay; the host's is used by both"};
inline constexpr Flag PREDICTION_FLAG{"--prediction", FlagKind::Value, "<ticks>", "how far to guess ahead; the host's is used by both"};
/// @brief Every flag a match over a network is described with. `NetworkSystem::FLAGS`.
inline constexpr std::array NETWORK_FLAGS{HOST_FLAG, JOIN_FLAG, LAG_FLAG, JITTER_FLAG, LOSS_FLAG, BOT_FLAG, DELAY_FLAG, PREDICTION_FLAG};
/// @brief What the command line asks of a match over a network, through `NETWORK_FLAGS`.
///
/// The delay is added to what each side sends, so the round trip is twice `--lag`. Arguments that are
/// not network flags are not an error here, since the same command line carries every system's.
struct NetworkOptions {
  bool host{};
  /// For a host, the port to listen on, or zero for any; for a joiner, the host's.
  uint16_t port{};
  std::string address;
  double lag{};
  double jitter{};
  float loss{};
  bool bot{};
  int inputDelay{2};
  int maxPrediction{8};
};
/// @brief Reads the options off a command line. Empty if it asks for no match over a network, and
/// then `error` says why if it was because of a mistake.
KUKI_ENGINE_API auto ParseNetworkOptions(const std::span<const std::string>, std::string &error) -> std::optional<NetworkOptions>;
} // namespace kuki
