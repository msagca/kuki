#pragma once
#include <cstddef>
#include <span>
#include <vector>
namespace kuki {
/// @brief A way of getting small messages to one other machine, and nothing more.
///
/// Each message is a datagram: it arrives whole or not at all, it may arrive late, twice, or after
/// one sent later, and nobody is told when it does not. That is what a UDP socket gives, and it is
/// the only thing the rollback session asks for. It does not want the retransmission and ordering a
/// stream adds, because both cost the one thing a fighting game is short of: a late input is worth
/// less than the next one, and waiting for it holds the next one back. The session sends every input
/// it has not heard acknowledged in each message instead, so a lost message is repaired by the one
/// after it, and does the ordering itself.
///
/// There is one peer. Who it is, how the two found each other and how the path between them was
/// opened are the business of whatever makes the object, not of anything that uses it.
class Transport {
public:
  virtual ~Transport() = default;
  /// @brief Sends one datagram and does not wait. A datagram that cannot be sent is dropped.
  virtual auto Send(std::span<const std::byte>) -> void = 0;
  /// @brief Takes the next datagram that has arrived into the vector, replacing what it held.
  ///
  /// False when nothing has, which is the usual answer and never a wait.
  virtual auto Receive(std::vector<std::byte> &) -> bool = 0;
};
} // namespace kuki
