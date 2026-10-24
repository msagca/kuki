#pragma once
#include <cstdint>
#include <functional>
#include <kuki_engine_export.h>
#include <transport.hpp>
#include <vector>
namespace kuki {
/// @brief Makes any `Transport` slower and worse than it is, for trying a game on a machine alone.
///
/// Two copies of a game on one PC talk over a wire with no delay at all, so a guess about the other's
/// buttons is hardly ever wrong and a rollback hardly ever happens, and everything the netcode is for
/// goes untried. This holds what is sent back for a while, loses some of it and lets the rest
/// overtake one another, as the internet does. Only what is sent is touched: put one at each end and
/// the delay is added both ways.
///
/// Time is read from a clock given to it, in seconds, so that a test can make time go by when it
/// likes. By default it is the steady clock.
class KUKI_ENGINE_API LaggedTransport final : public Transport {
public:
  struct Conditions {
    /// Seconds a datagram is held back, at least. One way: the round trip is twice this if both
    /// ends do the same.
    double latency{};
    /// Up to this many seconds more, chosen afresh for each datagram, which is also what reorders them.
    double jitter{};
    /// The share lost, from 0 to 1.
    float loss{};
    uint32_t seed{1};
  };
  using Clock = std::function<double()>;
  /// The transport must outlive this.
  LaggedTransport(Transport &, const Conditions &, Clock = {});
  auto Send(std::span<const std::byte>) -> void override;
  auto Receive(std::vector<std::byte> &) -> bool override;
  auto GetConditions() const -> const Conditions &;
private:
  struct Held {
    double releases{};
    uint64_t order{};
    std::vector<std::byte> bytes;
  };
  Transport &inner;
  Conditions conditions;
  Clock clock;
  std::vector<Held> held;
  uint32_t state;
  uint64_t counter{};
  auto Roll() -> float;
  /// @brief Sends on whatever has been held long enough, earliest first.
  auto Release() -> void;
};
} // namespace kuki
