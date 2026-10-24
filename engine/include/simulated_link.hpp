#pragma once
#include <cstddef>
#include <cstdint>
#include <kuki_engine_export.h>
#include <transport.hpp>
#include <vector>
namespace kuki {
/// @brief Two `Transport`s joined by a wire that is as bad as it is told to be.
///
/// For trying netcode without a network. The wire holds each datagram back for a while, may lose or
/// repeat it, and delivers in whatever order the delays put them in, which is the whole list of ways
/// the internet is unkind to a game. It runs on time that only moves when `Step` is called, and takes
/// its chances from a generator of its own that starts from a given seed, so a run that fails fails
/// the same way again. That is why it is the thing the session's tests stand on, and also why it is
/// public: two copies of a game in one process, joined by this, are how to watch a rollback happen
/// without a second machine.
///
/// The unit of time is whatever the caller makes it. One step a tick is the useful choice, which
/// makes `latency` a number of ticks.
class KUKI_ENGINE_API SimulatedLink {
public:
  struct Conditions {
    /// Steps a datagram takes to cross, at least.
    int latency{};
    /// Up to this many steps more, chosen afresh for every datagram, which is also what reorders them.
    int jitter{};
    /// The share of datagrams lost, from 0 to 1.
    float loss{};
    /// The share delivered twice, from 0 to 1. The copy takes its own delay.
    float duplicate{};
    uint32_t seed{1};
  };
  /// A wire with nothing wrong with it. Not a defaulted argument, which the compiler cannot form
  /// while the defaults of `Conditions` are still being read.
  SimulatedLink();
  explicit SimulatedLink(const Conditions &);
  SimulatedLink(const SimulatedLink &) = delete;
  auto operator=(const SimulatedLink &) -> SimulatedLink & = delete;
  /// @brief The two ends. What one sends the other receives.
  auto First() -> Transport &;
  auto Second() -> Transport &;
  /// @brief Lets one unit of time go by.
  auto Step() -> void;
  auto GetConditions() const -> const Conditions &;
  /// @brief Changes the conditions for datagrams sent from now on. What is already on the wire stays.
  auto SetConditions(const Conditions &) -> void;
  auto GetSent() const -> int;
  auto GetDropped() const -> int;
private:
  struct Datagram {
    int64_t arrives{};
    uint64_t order{};
    std::vector<std::byte> bytes;
  };
  class End final : public Transport {
  public:
    End(SimulatedLink &, const int);
    auto Send(std::span<const std::byte>) -> void override;
    auto Receive(std::vector<std::byte> &) -> bool override;
  private:
    SimulatedLink &link;
    int side;
  };
  Conditions conditions;
  End first;
  End second;
  /// Datagrams on the wire towards each end, indexed by the end they are going to.
  std::vector<Datagram> wire[2];
  int64_t now{};
  uint64_t counter{};
  uint32_t state;
  int sent{};
  int dropped{};
  /// A uniform number from 0 up to but not including 1.
  auto Roll() -> float;
  auto Launch(const int, std::span<const std::byte>) -> void;
};
} // namespace kuki
