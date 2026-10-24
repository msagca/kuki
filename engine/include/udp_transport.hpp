#pragma once
#include <cstdint>
#include <kuki_engine_export.h>
#include <memory>
#include <string_view>
#include <transport.hpp>
namespace kuki {
/// @brief A `Transport` over a UDP socket, to one other machine, with IPv4 addresses.
///
/// Never blocks: a socket that has nothing to read answers false, and one that cannot take a
/// datagram drops it, which is what the session expects of any transport.
///
/// Whoever waits is told nothing about who is coming. With no peer named, the first datagram to
/// arrive names one, and from then on datagrams from any other address are dropped. That is enough
/// to keep a stray packet from joining a match that has started, and not enough to keep one out of a
/// match that has not -- there is nothing here to tell a player from anybody else, and a game that
/// needs that has to say so over the connection.
class KUKI_ENGINE_API UdpTransport final : public Transport {
public:
  /// @brief Opens a socket on the given local port, or on any free one if it is zero.
  ///
  /// With a peer, datagrams go to it and only it is listened to. Without, it is learned. Null if
  /// the socket cannot be made or the name does not resolve, and the log says why.
  static auto Create(const uint16_t localPort, const std::string_view peerHost = {}, const uint16_t peerPort = 0) -> std::unique_ptr<UdpTransport>;
  ~UdpTransport() override;
  UdpTransport(const UdpTransport &) = delete;
  auto operator=(const UdpTransport &) -> UdpTransport & = delete;
  auto Send(std::span<const std::byte>) -> void override;
  auto Receive(std::vector<std::byte> &) -> bool override;
  /// @brief The port the socket is bound to, which is the one that was asked for unless it was zero.
  auto GetLocalPort() const -> uint16_t;
  auto HasPeer() const -> bool;
private:
  UdpTransport() = default;
  /// The socket, as the platform's own handle widened to fit either.
  uintptr_t handle{};
  uint16_t localPort{};
  /// The peer's address and port, in network byte order as the platform wants them.
  uint32_t peerAddress{};
  uint16_t peerPort{};
  bool peerKnown{};
};
} // namespace kuki
