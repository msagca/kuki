#include <algorithm>
#include <rollback.hpp>
#include <spdlog/spdlog.h>
using namespace kuki;
namespace {
constexpr uint16_t MAGIC = 0x4B52;
/// The most inputs one datagram carries. Sixty-four ticks is a second of loss the next datagram can
/// still repair on its own.
constexpr int MAX_REDUNDANCY = 64;
/// How many of the local player's inputs may be unacknowledged before the session stops making more.
/// Well under `INPUT_RING`, so that one still unacknowledged is never overwritten.
constexpr int MAX_UNACKED = 120;
/// A datagram: magic, ack, tick, advantage, checksum tick, checksum, first tick, count, and then
/// the inputs. Every number is little-endian whatever the machine is.
constexpr size_t HEADER_SIZE = 2 + 4 + 4 + 1 + 4 + 4 + 4 + 1;
struct Datagram {
  int64_t ack{-1};
  int64_t tick{};
  int advantage{};
  int64_t checksumTick{-1};
  uint32_t checksum{};
  int64_t first{};
  int count{};
  std::array<ButtonMask, MAX_REDUNDANCY> masks{};
};
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
auto GetTick(const std::span<const std::byte> in, size_t &at) -> int64_t {
  return static_cast<int32_t>(Get(in, at, 4));
}
auto Decode(const std::span<const std::byte> in, Datagram &out) -> bool {
  if (in.size() < HEADER_SIZE)
    return false;
  size_t at = 0;
  if (Get(in, at, 2) != MAGIC)
    return false;
  out.ack = GetTick(in, at);
  out.tick = GetTick(in, at);
  out.advantage = static_cast<int8_t>(Get(in, at, 1));
  out.checksumTick = GetTick(in, at);
  out.checksum = Get(in, at, 4);
  out.first = GetTick(in, at);
  out.count = static_cast<int>(Get(in, at, 1));
  if (out.count > MAX_REDUNDANCY || in.size() != HEADER_SIZE + static_cast<size_t>(out.count) * 4)
    return false;
  for (auto index = 0; index < out.count; ++index)
    out.masks[index] = Get(in, at, 4);
  return true;
}
/// FNV-1a. Not for security, only for noticing: two snapshots that differ by one bit hash apart.
auto Hash(const std::span<const std::byte> bytes) -> uint32_t {
  uint32_t hash = 2166136261u;
  for (const auto byte : bytes)
    hash = (hash ^ static_cast<uint32_t>(byte)) * 16777619u;
  return hash;
}
auto Slot(const int64_t tick) -> size_t {
  return static_cast<size_t>(tick) & (RollbackSession::INPUT_RING - 1);
}
} // namespace
RollbackSession::RollbackSession(RollbackGame &game, Transport &transport, const RollbackConfig &config)
  : game(game), transport(transport), config(config) {
  this->config.localPlayer = std::clamp(this->config.localPlayer, 0, 1);
  this->config.inputDelay = std::clamp(this->config.inputDelay, 0, 16);
  this->config.maxPrediction = std::clamp(this->config.maxPrediction, 0, MAX_PREDICTION);
  this->config.checksumInterval = std::max(this->config.checksumInterval, 0);
  remote = 1 - this->config.localPlayer;
  // One more than a rollback can reach back, and one more again for the tick about to be saved.
  snapshots.resize(static_cast<size_t>(this->config.maxPrediction) + 2);
  snapshotTicks.assign(snapshots.size(), -1);
  // The first ticks have no button to be pressed on them -- the player had not yet been asked --
  // so both machines treat them as neutral and the peer's copy of them is not waited for.
  lastLocal = this->config.inputDelay - 1;
}
auto RollbackSession::IsSessionDatagram(const std::span<const std::byte> bytes) -> bool {
  size_t at = 0;
  return bytes.size() >= HEADER_SIZE && Get(bytes, at, 2) == MAGIC;
}
auto RollbackSession::Step(const ButtonMask local) -> RollbackStep {
  Poll();
  if (disconnected)
    return {RollbackStatus::Disconnected, 0};
  auto ticks = Resimulate();
  UpdateChecksums();
  // Committed before deciding whether to run, and never replaced once it has been: the datagram
  // sent while stalled carries it, and the peer will run its copy of that tick on what it says.
  if (lastLocal < current + config.inputDelay) {
    lastLocal = current + config.inputDelay;
    localInputs[Slot(lastLocal)] = local;
  }
  if (current - received > config.maxPrediction || lastLocal - peerAck >= MAX_UNACKED) {
    ++stats.stalls;
    SendInputs();
    return {RollbackStatus::Stalled, ticks};
  }
  Save(current);
  Run(current, false);
  ++current;
  SendInputs();
  return {RollbackStatus::Ran, ticks + 1};
}
auto RollbackSession::GetTick() const -> int64_t {
  return current;
}
auto RollbackSession::GetConfirmedTick() const -> int64_t {
  return std::min(received, current - 1);
}
auto RollbackSession::LocalAdvantage() const -> int {
  return static_cast<int>(std::clamp<int64_t>(current - peerTick, -127, 127));
}
auto RollbackSession::GetAdvantage() const -> int {
  return heard ? (LocalAdvantage() - peerAdvantage) / 2 : 0;
}
auto RollbackSession::IsDesynced() const -> bool {
  return desyncTick >= 0;
}
auto RollbackSession::GetDesyncTick() const -> int64_t {
  return desyncTick;
}
auto RollbackSession::IsDisconnected() const -> bool {
  return disconnected;
}
auto RollbackSession::GetStats() const -> const RollbackStats & {
  return stats;
}
auto RollbackSession::GetConfig() const -> const RollbackConfig & {
  return config;
}
auto RollbackSession::Poll() -> void {
  ++silence;
  while (transport.Receive(inbox))
    Receive();
  if (silence > config.timeout)
    disconnected = true;
}
auto RollbackSession::Receive() -> void {
  Datagram datagram;
  if (!Decode(inbox, datagram))
    return;
  silence = 0;
  heard = true;
  peerAck = std::max(peerAck, datagram.ack);
  // The newest word wins. A datagram that arrives late says where the peer was, and not where it is.
  if (datagram.tick >= peerTick) {
    peerTick = datagram.tick;
    peerAdvantage = datagram.advantage;
  }
  // A datagram that starts after the next tick wanted would leave a gap, and inputs are taken in
  // order only. The sender always starts from what it last heard acknowledged, so this is not met
  // unless the datagram is damaged.
  if (datagram.first <= received + 1)
    for (auto index = 0; index < datagram.count; ++index) {
      const auto tick = datagram.first + index;
      if (tick <= received)
        continue;
      // A peer further ahead than the ring can hold is waited for, not stored over its own past.
      if (tick >= current + INPUT_RING / 2)
        break;
      remoteInputs[Slot(tick)] = datagram.masks[index];
      if (tick < current && usedRemote[Slot(tick)] != datagram.masks[index])
        rollbackTo = rollbackTo < 0 ? tick : std::min(rollbackTo, tick);
      received = tick;
    }
  if (datagram.checksumTick >= 0 && config.checksumInterval > 0) {
    theirs[static_cast<size_t>(datagram.checksumTick / config.checksumInterval) % CHECKSUM_SLOTS] = {datagram.checksumTick, datagram.checksum};
    Compare(datagram.checksumTick);
  }
}
auto RollbackSession::InputFor(const int player, const int64_t tick) const -> ButtonMask {
  if (player == config.localPlayer)
    return localInputs[Slot(tick)];
  if (tick <= received)
    return remoteInputs[Slot(tick)];
  // The guess: whatever the peer last did, it is still doing.
  return received >= 0 ? remoteInputs[Slot(received)] : 0u;
}
auto RollbackSession::Save(const int64_t tick) -> void {
  const auto slot = static_cast<size_t>(tick) % snapshots.size();
  snapshots[slot].clear();
  game.Save(snapshots[slot]);
  snapshotTicks[slot] = tick;
}
auto RollbackSession::Run(const int64_t tick, const bool replay) -> void {
  std::array<ButtonMask, 2> buttons{};
  for (auto player = 0; player < 2; ++player)
    buttons[player] = InputFor(player, tick);
  usedRemote[Slot(tick)] = buttons[remote];
  game.Advance(tick, buttons, replay);
}
auto RollbackSession::Resimulate() -> int {
  if (rollbackTo < 0)
    return 0;
  const auto target = rollbackTo;
  rollbackTo = -1;
  const auto slot = static_cast<size_t>(target) % snapshots.size();
  // The ring is one longer than a rollback can reach, so this is a bug if it is ever false, and
  // running on would leave the games different without anyone being told.
  if (snapshotTicks[slot] != target) {
    spdlog::error("[Rollback] No snapshot of tick {} to go back to, the game is at tick {}", target, current);
    if (desyncTick < 0)
      desyncTick = target;
    return 0;
  }
  game.Load(snapshots[slot]);
  for (auto tick = target; tick < current; ++tick) {
    // The state before the first is the one just loaded, and is already in the ring.
    if (tick > target)
      Save(tick);
    Run(tick, true);
  }
  const auto ticks = static_cast<int>(current - target);
  ++stats.rollbacks;
  stats.resimulated += ticks;
  stats.deepest = std::max(stats.deepest, ticks);
  return ticks;
}
auto RollbackSession::UpdateChecksums() -> void {
  if (config.checksumInterval <= 0)
    return;
  // The state before a tick is settled once every button before it is known, and it has to have
  // been saved, which the tick about to run has not.
  const auto settled = std::min(received + 1, current - 1);
  for (; nextChecksum <= settled; nextChecksum += config.checksumInterval) {
    const auto slot = static_cast<size_t>(nextChecksum) % snapshots.size();
    if (snapshotTicks[slot] != nextChecksum)
      continue;
    newest = {nextChecksum, Hash(snapshots[slot])};
    mine[static_cast<size_t>(nextChecksum / config.checksumInterval) % CHECKSUM_SLOTS] = newest;
    Compare(nextChecksum);
  }
}
auto RollbackSession::Compare(const int64_t tick) -> void {
  const auto slot = static_cast<size_t>(tick / config.checksumInterval) % CHECKSUM_SLOTS;
  if (mine[slot].tick != tick || theirs[slot].tick != tick)
    return;
  // Every datagram repeats the newest summary, so the same tick is met again and again.
  if (tick > comparedThrough) {
    comparedThrough = tick;
    ++stats.checksumsCompared;
  }
  if (mine[slot].hash == theirs[slot].hash || desyncTick >= 0)
    return;
  desyncTick = tick;
  spdlog::error("[Rollback] The games disagree about the state before tick {}: {:08x} here, {:08x} at the peer", tick, mine[slot].hash, theirs[slot].hash);
}
auto RollbackSession::SendInputs() -> void {
  const auto first = peerAck + 1;
  const auto count = static_cast<int>(std::clamp<int64_t>(lastLocal - first + 1, 0, MAX_REDUNDANCY));
  outbox.clear();
  Put(outbox, MAGIC, 2);
  Put(outbox, static_cast<uint32_t>(static_cast<int32_t>(received)), 4);
  Put(outbox, static_cast<uint32_t>(static_cast<int32_t>(current)), 4);
  Put(outbox, static_cast<uint32_t>(static_cast<int8_t>(LocalAdvantage())) & 0xFFu, 1);
  Put(outbox, static_cast<uint32_t>(static_cast<int32_t>(newest.tick)), 4);
  Put(outbox, newest.hash, 4);
  Put(outbox, static_cast<uint32_t>(static_cast<int32_t>(first)), 4);
  Put(outbox, static_cast<uint32_t>(count), 1);
  for (auto index = 0; index < count; ++index)
    Put(outbox, localInputs[Slot(first + index)], 4);
  transport.Send(outbox);
}
