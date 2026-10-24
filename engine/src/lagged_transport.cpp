#include <algorithm>
#include <chrono>
#include <lagged_transport.hpp>
#include <utility>
using namespace kuki;
namespace {
auto SteadySeconds() -> double {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace
LaggedTransport::LaggedTransport(Transport &inner, const Conditions &conditions, Clock clock)
  : inner(inner), conditions(conditions), clock(clock ? std::move(clock) : Clock(SteadySeconds)), state(conditions.seed ? conditions.seed : 1u) {}
auto LaggedTransport::GetConditions() const -> const Conditions & {
  return conditions;
}
auto LaggedTransport::Roll() -> float {
  // The same generator as `SimulatedLink`, for the same reason: a run that goes wrong should go
  // wrong again.
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return static_cast<float>(state >> 8) / static_cast<float>(1u << 24);
}
auto LaggedTransport::Send(const std::span<const std::byte> bytes) -> void {
  const auto lose = Roll();
  const auto spread = Roll();
  if (lose >= conditions.loss)
    held.push_back({clock() + conditions.latency + conditions.jitter * static_cast<double>(spread), counter++, {bytes.begin(), bytes.end()}});
  Release();
}
auto LaggedTransport::Receive(std::vector<std::byte> &out) -> bool {
  // Releasing here as well as on a send is what lets a datagram go out on time when nothing else is
  // being sent, which is the case a game that stalls is in.
  Release();
  return inner.Receive(out);
}
auto LaggedTransport::Release() -> void {
  const auto now = clock();
  std::ranges::stable_sort(held, [](const Held &a, const Held &b) { return a.releases != b.releases ? a.releases < b.releases : a.order < b.order; });
  auto ready = held.begin();
  while (ready != held.end() && ready->releases <= now) {
    inner.Send(ready->bytes);
    ++ready;
  }
  held.erase(held.begin(), ready);
}
