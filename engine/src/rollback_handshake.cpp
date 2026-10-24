#include <algorithm>
#include <rollback_handshake.hpp>
#include <spdlog/spdlog.h>
using namespace kuki;
namespace {
constexpr uint16_t MAGIC = 0x4B48;
enum Type : uint8_t { Hello = 1,
  Welcome,
  Ready,
  Reject };
/// Magic, type and protocol, and for a welcome the settings after them.
constexpr size_t BASE_SIZE = 2 + 1 + 4;
constexpr size_t WELCOME_SIZE = BASE_SIZE + 1 + 1 + 1 + 2;
auto Put(std::vector<std::byte> &out, const uint32_t value, const int bytes) -> void {
  for (auto index = 0; index < bytes; ++index)
    out.push_back(static_cast<std::byte>(value >> (8 * index) & 0xFFu));
}
auto Get(const std::span<const std::byte> in, size_t &at, const int bytes) -> uint32_t {
  uint32_t value{};
  for (auto index = 0; index < bytes; ++index)
    value |= static_cast<uint32_t>(in[at++]) << (8 * index);
  return value;
}
} // namespace
RollbackHandshake::RollbackHandshake(Transport &transport, const Role role, const Options &options)
  : transport(transport), role(role), options(options) {
  result.localPlayer = role == Role::Host ? 0 : 1;
  result.tickRate = options.tickRate;
  result.config = options.config;
  result.config.localPlayer = result.localPlayer;
}
auto RollbackHandshake::GetStatus() const -> Status {
  return status;
}
auto RollbackHandshake::GetResult() const -> const HandshakeResult & {
  return result;
}
auto RollbackHandshake::GetFailure() const -> const std::string & {
  return failure;
}
auto RollbackHandshake::Fail(std::string reason) -> void {
  spdlog::error("[Handshake] {}", reason);
  failure = std::move(reason);
  status = Status::Failed;
}
auto RollbackHandshake::Poll(const double seconds) -> Status {
  if (status != Status::Waiting)
    return status;
  sinceSent += seconds;
  // A host is not on the clock until somebody has called, and a joiner is from the start.
  if (role == Role::Join || heard)
    waited += seconds;
  while (status == Status::Waiting && transport.Receive(inbox))
    Handle();
  if (status != Status::Waiting)
    return status;
  if (waited > options.timeout) {
    Fail(role == Role::Join ? "The host did not answer" : "The other player called but never confirmed");
    return status;
  }
  if (role == Role::Join && (!sentYet || sinceSent >= options.resend)) {
    SendHello();
    sinceSent = 0.;
    sentYet = true;
  } else if (role == Role::Host && heard && sinceSent >= options.resend) {
    SendWelcome();
    sinceSent = 0.;
  }
  return status;
}
auto RollbackHandshake::Handle() -> void {
  if (RollbackSession::IsSessionDatagram(inbox)) {
    // The peer has begun, so it has been through all of this; what is left of it is not needed.
    if (role == Role::Host && heard)
      status = Status::Ready;
    return;
  }
  if (inbox.size() < BASE_SIZE)
    return;
  size_t at = 0;
  if (Get(inbox, at, 2) != MAGIC)
    return;
  const auto type = static_cast<Type>(Get(inbox, at, 1));
  const auto protocol = Get(inbox, at, 4);
  if (protocol != options.protocol) {
    if (role == Role::Host && type == Hello)
      SendReject();
    else if (role == Role::Join && (type == Welcome || type == Reject))
      Fail("The host is running a different version of the game");
    return;
  }
  if (role == Role::Host) {
    if (type == Hello) {
      heard = true;
      SendWelcome();
      sinceSent = 0.;
    } else if (type == Ready && heard)
      status = Status::Ready;
    return;
  }
  if (type == Reject) {
    Fail("The host is running a different version of the game");
    return;
  }
  if (type != Welcome || inbox.size() < WELCOME_SIZE)
    return;
  const auto tickRate = static_cast<int>(Get(inbox, at, 1));
  const auto inputDelay = static_cast<int>(Get(inbox, at, 1));
  const auto maxPrediction = static_cast<int>(Get(inbox, at, 1));
  const auto checksumInterval = static_cast<int>(Get(inbox, at, 2));
  if (tickRate < 1) {
    Fail("The host sent a tick rate of nothing");
    return;
  }
  result.tickRate = tickRate;
  result.config.inputDelay = inputDelay;
  result.config.maxPrediction = maxPrediction;
  result.config.checksumInterval = checksumInterval;
  // Three, since this is said once: if every one is lost the host goes on welcoming, and the first
  // datagram of the session tells it the rest.
  for (auto copy = 0; copy < 3; ++copy)
    SendReady();
  status = Status::Ready;
}
auto RollbackHandshake::SendHello() -> void {
  outbox.clear();
  Put(outbox, MAGIC, 2);
  Put(outbox, Hello, 1);
  Put(outbox, options.protocol, 4);
  transport.Send(outbox);
}
auto RollbackHandshake::SendWelcome() -> void {
  outbox.clear();
  Put(outbox, MAGIC, 2);
  Put(outbox, Welcome, 1);
  Put(outbox, options.protocol, 4);
  Put(outbox, static_cast<uint32_t>(std::clamp(options.tickRate, 1, 255)), 1);
  Put(outbox, static_cast<uint32_t>(std::clamp(options.config.inputDelay, 0, 255)), 1);
  Put(outbox, static_cast<uint32_t>(std::clamp(options.config.maxPrediction, 0, 255)), 1);
  Put(outbox, static_cast<uint32_t>(std::clamp(options.config.checksumInterval, 0, 65535)), 2);
  transport.Send(outbox);
}
auto RollbackHandshake::SendReady() -> void {
  outbox.clear();
  Put(outbox, MAGIC, 2);
  Put(outbox, Ready, 1);
  Put(outbox, options.protocol, 4);
  transport.Send(outbox);
}
auto RollbackHandshake::SendReject() -> void {
  outbox.clear();
  Put(outbox, MAGIC, 2);
  Put(outbox, Reject, 1);
  Put(outbox, options.protocol, 4);
  transport.Send(outbox);
}
