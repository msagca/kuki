#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <kuki_engine_export.h>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
namespace kuki {
/// @brief Index standing in for "no parent", which is what a scope opened directly inside a frame gets.
inline constexpr size_t PROFILE_ROOT = static_cast<size_t>(-1);
/// @brief Frames the frame-time plot keeps. At sixty frames a second this is four seconds of history.
inline constexpr size_t PROFILE_FRAME_HISTORY = 240;
/// @brief How deep scopes may nest before further ones are counted and dropped.
///
/// The stack is a fixed array so that entering a scope never allocates, which is what makes the
/// profiler safe to leave sitting in hot code. Anything past this depth is a scope that recurses
/// indirectly, not a real call tree, so dropping it is better than growing to fit it.
inline constexpr size_t PROFILE_MAX_DEPTH = 32;
/// @brief Marks one frame may record before further ones are dropped.
inline constexpr size_t PROFILE_MAX_MARKS = 256;
/// @brief One scope in the call tree, identified by its name *and* the path of scopes above it.
///
/// The same name under two different parents is two nodes, so a scope inside a helper that several
/// callers share reports once per caller rather than averaging them into a single number.
struct ProfileNode {
  std::string name;
  size_t parent{PROFILE_ROOT};
  std::vector<size_t> children;
  uint32_t depth{};
  /// @brief Times the scope was entered during the last completed frame.
  uint32_t lastCalls{};
  /// @brief Inclusive milliseconds the scope took over the last completed frame.
  double lastMillis{};
  /// @brief Mean of `lastMillis` across the frames the scope was actually entered in.
  ///
  /// Frames that never touch the scope are left out of the divisor. Averaging a load-time cost over
  /// every frame since startup reports a number that no frame ever paid.
  double avgMillis{};
  /// @brief Worst single frame since the last reset, and the frame index it happened on.
  ///
  /// This is the pair that catches a one-off stall. Work that costs a second once and nothing
  /// afterwards leaves almost no trace in an average, and a live per-frame view has already
  /// scrolled past it by the time anyone looks.
  double maxMillis{};
  uint64_t maxFrame{};
  uint64_t totalCalls{};
  uint64_t sampledFrames{};
  double totalMillis{};
  /// @brief Accumulators for the frame in progress, folded into the fields above by `EndFrame`.
  uint32_t calls{};
  int64_t nanos{};
  uint32_t recursion{};
};
/// @brief A named instant within a frame, for questions a scope cannot answer.
///
/// A scope measures a region; a mark answers "how far into the frame did this happen", which is what
/// you want when the thing you are chasing is an event rather than a cost.
struct ProfileMark {
  std::string name;
  double millis{};
};
/// @brief Wall-clock CPU timing for named scopes, aggregated per frame into a call tree.
///
/// One instance, reached through `Get`, because the scopes it collects sit in engine internals that
/// have no business being handed a profiler. `Application` drives `BeginFrame` and `EndFrame`; the
/// rest of the engine only ever opens scopes.
///
/// Only the thread that opened the first frame is sampled. Timing an asset load on a worker thread
/// would need per-thread stacks and a lock around the tree, and neither is worth paying for on
/// every scope when the question this answers is where the frame went. Calls from other threads are
/// ignored rather than mangling the tree, so leaving a scope in code that both threads run is safe.
class KUKI_ENGINE_API Profiler {
public:
  static auto Get() -> Profiler &;
  /// @brief Starts a frame, discarding any scope left open by the previous one.
  auto BeginFrame() -> void;
  /// @brief Closes the frame and folds its scopes into the running statistics.
  auto EndFrame() -> void;
  /// @brief Opens a scope as a child of whichever scope is currently open.
  ///
  /// A scope that re-enters itself directly is attributed to the one node and timed by its
  /// outermost entry, so a recursive function reports the cost of the whole recursion once instead
  /// of a tree one node deep per level.
  auto Begin(std::string_view) -> void;
  auto End() -> void;
  auto Mark(std::string_view) -> void;
  /// @brief Drops every node, mark and statistic, including the frame history.
  auto Reset() -> void;
  auto IsEnabled() const -> bool;
  auto SetEnabled(bool) -> void;
  /// @brief Freezes the statistics for reading. Scopes become no-ops until it is cleared.
  auto IsPaused() const -> bool;
  auto SetPaused(bool) -> void;
  auto GetNodes() const -> const std::vector<ProfileNode> &;
  auto GetRoots() const -> const std::vector<size_t> &;
  /// @brief Marks from the most recent frame that recorded any, which is not necessarily the last one.
  auto GetMarks() const -> const std::vector<ProfileMark> &;
  auto GetMarkFrame() const -> uint64_t;
  auto GetFrameMillis() const -> float;
  /// @brief The frame-time ring buffer, sized `PROFILE_FRAME_HISTORY` and oldest-first from the offset.
  auto GetFrameHistory() const -> const std::vector<float> &;
  auto GetFrameHistoryOffset() const -> size_t;
  /// @brief Frames completed since the last reset. The frame in progress is this index.
  auto GetFrameIndex() const -> uint64_t;
private:
  using Clock = std::chrono::steady_clock;
  struct StackEntry {
    size_t node{PROFILE_ROOT};
    Clock::time_point start{};
    bool reentrant{};
  };
  Profiler();
  auto Sampling() const -> bool;
  auto GetOrCreateNode(size_t, std::string_view) -> size_t;
  std::vector<ProfileNode> nodes;
  std::vector<size_t> roots;
  std::vector<ProfileMark> marks;
  std::vector<ProfileMark> frameMarks;
  std::vector<float> frameHistory;
  std::array<StackEntry, PROFILE_MAX_DEPTH> stack{};
  Clock::time_point frameStart{};
  std::thread::id owner{};
  uint64_t frameIndex{};
  uint64_t markFrame{};
  size_t historyOffset{};
  size_t depth{};
  size_t overflow{};
  float frameMillis{};
  bool enabled{true};
  bool paused{};
  bool inFrame{};
};
/// @brief Opens a profiler scope for as long as it is alive.
struct ProfileScope {
  explicit ProfileScope(const std::string_view name) {
    Profiler::Get().Begin(name);
  }
  ~ProfileScope() {
    Profiler::Get().End();
  }
  ProfileScope(const ProfileScope &) = delete;
  ProfileScope(ProfileScope &&) = delete;
  auto operator=(const ProfileScope &) -> ProfileScope & = delete;
  auto operator=(ProfileScope &&) -> ProfileScope & = delete;
};
} // namespace kuki
#ifdef KUKI_DISABLE_PROFILING
#define KUKI_PROFILE_SCOPE(name) ((void)0)
#define KUKI_PROFILE_FUNCTION() ((void)0)
#define KUKI_PROFILE_MARK(name) ((void)0)
#else
#define KUKI_PROFILE_CONCAT_IMPL(a, b) a##b
#define KUKI_PROFILE_CONCAT(a, b) KUKI_PROFILE_CONCAT_IMPL(a, b)
/// @brief Times the enclosing block under `name`, which must outlive the block if it is a pointer.
#define KUKI_PROFILE_SCOPE(name) const kuki::ProfileScope KUKI_PROFILE_CONCAT(profileScope, __LINE__)(name)
#define KUKI_PROFILE_FUNCTION() KUKI_PROFILE_SCOPE(__func__)
#define KUKI_PROFILE_MARK(name) kuki::Profiler::Get().Mark(name)
#endif
