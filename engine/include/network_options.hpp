#pragma once
#include <cstdint>
#include <kuki_engine_export.h>
#include <optional>
#include <span>
#include <string>
namespace kuki {
/// @brief What the command line asks of a match over a network.
///
///   `--host <port>`        wait for the other player to call on this port
///   `--join <host:port>`   call the host
///   `--lag <ms>`           hold back what is sent by this long, so one PC can stand in for the internet
///   `--jitter <ms>`        and by up to this much more, which also reorders it
///   `--loss <percent>`     and lose this share of it
///   `--bot`                play from a script instead of the keyboard, for the window that is not focused
///   `--delay <ticks>`      input delay; the host's is used by both
///   `--prediction <ticks>` how far to guess ahead; the host's is used by both
///
/// The delay is added to what each side sends, so the round trip is twice `--lag`. Arguments that are
/// not in this list are not an error, since a game's executable is also given the editor's.
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
