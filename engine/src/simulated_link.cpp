#include <algorithm>
#include <simulated_link.hpp>
#include <utility>
using namespace kuki;
SimulatedLink::End::End(SimulatedLink &link, const int side)
  : link(link), side(side) {}
auto SimulatedLink::End::Send(const std::span<const std::byte> bytes) -> void {
  // Towards the other end: what one side sends is what the opposite side finds waiting.
  link.Launch(1 - side, bytes);
}
auto SimulatedLink::End::Receive(std::vector<std::byte> &out) -> bool {
  auto &queue = link.wire[side];
  // The earliest that has arrived, ties going to the one sent first. The wire is not kept sorted
  // because it is never long; a game sends one datagram a tick and each is gone within a few.
  auto best = queue.end();
  for (auto it = queue.begin(); it != queue.end(); ++it)
    if (it->arrives <= link.now && (best == queue.end() || it->arrives < best->arrives || (it->arrives == best->arrives && it->order < best->order)))
      best = it;
  if (best == queue.end())
    return false;
  out = std::move(best->bytes);
  queue.erase(best);
  return true;
}
SimulatedLink::SimulatedLink()
  : SimulatedLink(Conditions{}) {}
SimulatedLink::SimulatedLink(const Conditions &conditions)
  : conditions(conditions), first(*this, 0), second(*this, 1), state(conditions.seed ? conditions.seed : 1u) {}
auto SimulatedLink::First() -> Transport & {
  return first;
}
auto SimulatedLink::Second() -> Transport & {
  return second;
}
auto SimulatedLink::Step() -> void {
  ++now;
}
auto SimulatedLink::GetConditions() const -> const Conditions & {
  return conditions;
}
auto SimulatedLink::SetConditions(const Conditions &value) -> void {
  conditions = value;
}
auto SimulatedLink::GetSent() const -> int {
  return sent;
}
auto SimulatedLink::GetDropped() const -> int {
  return dropped;
}
auto SimulatedLink::Roll() -> float {
  // xorshift32. Nothing about the quality of the numbers matters here; that they are the same ones
  // on every machine and every run does, which `std::uniform_real_distribution` does not promise.
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return static_cast<float>(state >> 8) / static_cast<float>(1u << 24);
}
auto SimulatedLink::Launch(const int towards, const std::span<const std::byte> bytes) -> void {
  ++sent;
  // Every roll is taken whether or not it is needed, so that changing one condition does not change
  // which datagrams the others fall on.
  const auto lose = Roll();
  const auto repeat = Roll();
  if (lose < conditions.loss) {
    ++dropped;
    return;
  }
  for (auto copy = 0; copy < (repeat < conditions.duplicate ? 2 : 1); ++copy) {
    const auto extra = conditions.jitter > 0 ? static_cast<int>(Roll() * static_cast<float>(conditions.jitter + 1)) : 0;
    wire[towards].push_back({now + conditions.latency + std::min(extra, conditions.jitter), counter++, {bytes.begin(), bytes.end()}});
  }
}
