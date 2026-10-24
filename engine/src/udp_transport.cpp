#include <array>
#include <spdlog/spdlog.h>
#include <string>
#include <udp_transport.hpp>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
using namespace kuki;
namespace {
#ifdef _WIN32
using Handle = SOCKET;
constexpr Handle INVALID = INVALID_SOCKET;
// Asks Windows not to turn an ICMP "port unreachable" -- a datagram sent to a peer that is not
// listening yet -- into an error on the next read from the socket. Without it, a joiner that starts
// before its host has opened the port makes the joiner's own socket fail, which is not what
// anybody means by "nothing has arrived".
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
auto Startup() -> bool {
  static const auto started = [] {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }();
  return started;
}
auto Configure(const Handle socket) -> bool {
  u_long nonBlocking = 1;
  if (ioctlsocket(socket, FIONBIO, &nonBlocking) != 0)
    return false;
  DWORD bytes = 0;
  BOOL reset = FALSE;
  WSAIoctl(socket, SIO_UDP_CONNRESET, &reset, sizeof(reset), nullptr, 0, &bytes, nullptr, nullptr);
  return true;
}
auto Close(const Handle socket) -> void {
  closesocket(socket);
}
/// Whether a failed read is only the platform reporting on an earlier send, and says nothing about
/// whether there is more to read.
auto IsIgnorable() -> bool {
  const auto error = WSAGetLastError();
  return error == WSAECONNRESET || error == WSAEMSGSIZE;
}
#else
using Handle = int;
constexpr Handle INVALID = -1;
auto Startup() -> bool {
  return true;
}
auto Configure(const Handle socket) -> bool {
  const auto flags = fcntl(socket, F_GETFL, 0);
  return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
}
auto Close(const Handle socket) -> void {
  close(socket);
}
auto IsIgnorable() -> bool {
  return errno == ECONNREFUSED || errno == EMSGSIZE;
}
#endif
/// Larger than any datagram the session or the handshake sends, with room to spare.
constexpr size_t MAX_DATAGRAM = 2048;
} // namespace
auto UdpTransport::Create(const uint16_t localPort, const std::string_view peerHost, const uint16_t peerPort) -> std::unique_ptr<UdpTransport> {
  if (!Startup()) {
    spdlog::error("[UdpTransport] The socket library did not start");
    return nullptr;
  }
  sockaddr_in peer{};
  if (!peerHost.empty()) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo *found = nullptr;
    const std::string host(peerHost);
    if (getaddrinfo(host.c_str(), nullptr, &hints, &found) != 0 || !found) {
      spdlog::error("[UdpTransport] Cannot find {}", host);
      return nullptr;
    }
    peer = *reinterpret_cast<const sockaddr_in *>(found->ai_addr);
    freeaddrinfo(found);
  }
  const Handle socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socketHandle == INVALID || !Configure(socketHandle)) {
    spdlog::error("[UdpTransport] Cannot make a socket");
    if (socketHandle != INVALID)
      Close(socketHandle);
    return nullptr;
  }
  sockaddr_in local{};
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = htonl(INADDR_ANY);
  local.sin_port = htons(localPort);
  if (bind(socketHandle, reinterpret_cast<const sockaddr *>(&local), sizeof(local)) != 0) {
    spdlog::error("[UdpTransport] Cannot listen on port {}; is something else using it?", localPort);
    Close(socketHandle);
    return nullptr;
  }
  socklen_t length = sizeof(local);
  getsockname(socketHandle, reinterpret_cast<sockaddr *>(&local), &length);
  // The constructor is private so that a half-made one cannot be had.
  std::unique_ptr<UdpTransport> transport(new UdpTransport);
  transport->handle = static_cast<uintptr_t>(socketHandle);
  transport->localPort = ntohs(local.sin_port);
  if (!peerHost.empty()) {
    transport->peerAddress = peer.sin_addr.s_addr;
    transport->peerPort = htons(peerPort);
    transport->peerKnown = true;
  }
  return transport;
}
UdpTransport::~UdpTransport() {
  Close(static_cast<Handle>(handle));
}
auto UdpTransport::Send(const std::span<const std::byte> bytes) -> void {
  if (!peerKnown)
    return;
  sockaddr_in to{};
  to.sin_family = AF_INET;
  to.sin_addr.s_addr = peerAddress;
  to.sin_port = peerPort;
  sendto(static_cast<Handle>(handle), reinterpret_cast<const char *>(bytes.data()), static_cast<int>(bytes.size()), 0, reinterpret_cast<const sockaddr *>(&to), sizeof(to));
}
auto UdpTransport::Receive(std::vector<std::byte> &out) -> bool {
  std::array<char, MAX_DATAGRAM> buffer;
  for (;;) {
    sockaddr_in from{};
    socklen_t length = sizeof(from);
    const auto count = recvfrom(static_cast<Handle>(handle), buffer.data(), static_cast<int>(buffer.size()), 0, reinterpret_cast<sockaddr *>(&from), &length);
    if (count < 0) {
      if (IsIgnorable())
        continue;
      return false;
    }
    if (!peerKnown) {
      peerAddress = from.sin_addr.s_addr;
      peerPort = from.sin_port;
      peerKnown = true;
    } else if (from.sin_addr.s_addr != peerAddress || from.sin_port != peerPort)
      continue;
    const auto *bytes = reinterpret_cast<const std::byte *>(buffer.data());
    out.assign(bytes, bytes + count);
    return true;
  }
}
auto UdpTransport::GetLocalPort() const -> uint16_t {
  return localPort;
}
auto UdpTransport::HasPeer() const -> bool {
  return peerKnown;
}
