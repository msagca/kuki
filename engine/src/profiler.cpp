#include <chrono>
#include <profiler.hpp>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
namespace kuki {
namespace {
  constexpr double NANOS_PER_MILLI = 1000000.;
}
Profiler::Profiler() {
  frameHistory.assign(PROFILE_FRAME_HISTORY, .0f);
}
auto Profiler::Get() -> Profiler & {
  static Profiler profiler;
  return profiler;
}
auto Profiler::Sampling() const -> bool {
  return inFrame && std::this_thread::get_id() == owner;
}
auto Profiler::GetOrCreateNode(const size_t parent, const std::string_view name) -> size_t {
  const auto &siblings = parent == PROFILE_ROOT ? roots : nodes[parent].children;
  for (const auto index : siblings)
    if (std::string_view(nodes[index].name) == name)
      return index;
  const auto index = nodes.size();
  ProfileNode node;
  node.name = std::string(name);
  node.parent = parent;
  node.depth = parent == PROFILE_ROOT ? 0u : nodes[parent].depth + 1u;
  nodes.push_back(std::move(node));
  if (parent == PROFILE_ROOT)
    roots.push_back(index);
  else
    nodes[parent].children.push_back(index);
  return index;
}
auto Profiler::BeginFrame() -> void {
  if (owner == std::thread::id{})
    owner = std::this_thread::get_id();
  if (!enabled || paused || std::this_thread::get_id() != owner)
    return;
  depth = 0;
  overflow = 0;
  marks.clear();
  for (auto &node : nodes) {
    node.calls = 0;
    node.nanos = 0;
    node.recursion = 0;
  }
  frameStart = Clock::now();
  inFrame = true;
}
auto Profiler::EndFrame() -> void {
  if (!Sampling())
    return;
  inFrame = false;
  const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - frameStart).count();
  frameMillis = static_cast<float>(static_cast<double>(elapsed) / NANOS_PER_MILLI);
  frameHistory[historyOffset] = frameMillis;
  historyOffset = (historyOffset + 1) % frameHistory.size();
  for (auto &node : nodes) {
    if (node.calls == 0) {
      node.lastCalls = 0;
      node.lastMillis = .0;
      continue;
    }
    const auto millis = static_cast<double>(node.nanos) / NANOS_PER_MILLI;
    node.lastCalls = node.calls;
    node.lastMillis = millis;
    node.totalCalls += node.calls;
    node.totalMillis += millis;
    ++node.sampledFrames;
    node.avgMillis = node.totalMillis / static_cast<double>(node.sampledFrames);
    if (millis > node.maxMillis) {
      node.maxMillis = millis;
      node.maxFrame = frameIndex;
    }
  }
  if (!marks.empty()) {
    frameMarks = marks;
    markFrame = frameIndex;
  }
  ++frameIndex;
  depth = 0;
  overflow = 0;
}
auto Profiler::Begin(const std::string_view name) -> void {
  if (!Sampling())
    return;
  if (depth >= PROFILE_MAX_DEPTH) {
    ++overflow;
    return;
  }
  if (depth > 0) {
    const auto current = stack[depth - 1].node;
    if (std::string_view(nodes[current].name) == name) {
      ++nodes[current].recursion;
      stack[depth] = {current, Clock::now(), true};
      ++depth;
      return;
    }
  }
  const auto parent = depth > 0 ? stack[depth - 1].node : PROFILE_ROOT;
  const auto index = GetOrCreateNode(parent, name);
  stack[depth] = {index, Clock::now(), false};
  ++depth;
}
auto Profiler::End() -> void {
  if (!Sampling())
    return;
  if (overflow > 0) {
    --overflow;
    return;
  }
  if (depth == 0)
    return;
  --depth;
  const auto &entry = stack[depth];
  auto &node = nodes[entry.node];
  ++node.calls;
  if (entry.reentrant) {
    if (node.recursion > 0)
      --node.recursion;
    return;
  }
  node.nanos += std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - entry.start).count();
}
auto Profiler::Mark(const std::string_view name) -> void {
  if (!Sampling() || marks.size() >= PROFILE_MAX_MARKS)
    return;
  const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - frameStart).count();
  marks.push_back({std::string(name), static_cast<double>(elapsed) / NANOS_PER_MILLI});
}
auto Profiler::Reset() -> void {
  nodes.clear();
  roots.clear();
  marks.clear();
  frameMarks.clear();
  frameHistory.assign(PROFILE_FRAME_HISTORY, .0f);
  historyOffset = 0;
  frameIndex = 0;
  markFrame = 0;
  depth = 0;
  overflow = 0;
  frameMillis = .0f;
  inFrame = false;
}
auto Profiler::IsEnabled() const -> bool {
  return enabled;
}
auto Profiler::SetEnabled(const bool value) -> void {
  enabled = value;
  if (!enabled)
    inFrame = false;
}
auto Profiler::IsPaused() const -> bool {
  return paused;
}
auto Profiler::SetPaused(const bool value) -> void {
  paused = value;
  if (paused)
    inFrame = false;
}
auto Profiler::GetNodes() const -> const std::vector<ProfileNode> & {
  return nodes;
}
auto Profiler::GetRoots() const -> const std::vector<size_t> & {
  return roots;
}
auto Profiler::GetMarks() const -> const std::vector<ProfileMark> & {
  return frameMarks;
}
auto Profiler::GetMarkFrame() const -> uint64_t {
  return markFrame;
}
auto Profiler::GetFrameMillis() const -> float {
  return frameMillis;
}
auto Profiler::GetFrameHistory() const -> const std::vector<float> & {
  return frameHistory;
}
auto Profiler::GetFrameHistoryOffset() const -> size_t {
  return historyOffset;
}
auto Profiler::GetFrameIndex() const -> uint64_t {
  return frameIndex;
}
} // namespace kuki
