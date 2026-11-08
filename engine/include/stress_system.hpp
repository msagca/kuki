#pragma once
#include <array>
#include <command_line.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <optional>
#include <string>
#include <string_view>
#include <system.hpp>
#include <vector>
namespace kuki {
/// @brief Ramps a load up in steps, reports each step's frame times and quits -- with 1 if the full
/// load misses the budget, so a script can tell. Read by `StressSystem`.
inline constexpr Flag STRESS_FLAG{"--stress", FlagKind::Value, "<load>", "ramp up a load (draws, rigs, lights, particles or all), log each step's frame times and quit: 1 if the full load is over budget"};
inline constexpr Flag STRESS_SCALE_FLAG{"--stress-scale", FlagKind::Value, "<factor>", "how many times the --stress load to ramp up to (1)"};
inline constexpr Flag STRESS_BUDGET_FLAG{"--stress-budget", FlagKind::Value, "<ms>", "the frame time 95 frames in 100 of the full --stress load must keep within (16.7)"};
inline constexpr Flag STRESS_REPORT_FLAG{"--stress-report", FlagKind::Value, "<file.json>", "also write the --stress steps to this file"};
/// @brief What a stress test puts into the active scene, all of it under one entity of its own.
///
/// Every kind is made of what a game makes its scenes of, so that what it costs is what a game's
/// scene of that size would: `instances` are mesh objects spread over `pools` pools (the stress
/// test's meshes times its materials), `rigs` are skinned figures playing a clip, `lights` are
/// point and spot lights, half each, and `emitters` are particle emitters of the default kind.
struct StressLoad {
  uint32_t instances{};
  uint32_t pools{1};
  uint32_t rigs{};
  uint32_t lights{};
  uint32_t emitters{};
  /// @brief Whether every instance turns each frame, as a game's moving things do: each is a
  /// transform marked dirty and a world matrix uploaded again, which a still field never pays.
  bool moving{true};
  auto operator==(const StressLoad &) const -> bool = default;
};
/// @brief The loads `--stress` names, at a scale of one, as `{name, load}`.
///
/// Each is sized to be well past what a game of this engine's puts on screen -- a fighting game's
/// stage is a few hundred instances, two figures and a handful of lights -- so that it finds where
/// the time goes rather than confirming there is time to spare. Lights stop at what the renderer
/// shades, eight of each kind; more are drawn by nothing.
struct StressPreset {
  std::string_view name;
  StressLoad load;
};
inline constexpr std::array STRESS_PRESETS{
  StressPreset{"draws", {.instances = 20000, .pools = 16}},
  StressPreset{"rigs", {.rigs = 128}},
  StressPreset{"lights", {.lights = 16}},
  StressPreset{"particles", {.emitters = 32}},
  StressPreset{"all", {.instances = 20000, .pools = 16, .rigs = 128, .lights = 16, .emitters = 32}},
};
/// @brief The load of a preset by its name, if one has it.
KUKI_ENGINE_API auto FindStressPreset(std::string_view) -> std::optional<StressLoad>;
/// @brief One profiler scope's share of a measured step, inclusive of the scopes inside it.
struct StressScope {
  /// @brief The scope's name with every scope above it, `Systems/Animation`.
  std::string path;
  double millis{};
};
/// @brief What one step of a ramp measured.
struct StressStep {
  StressLoad load;
  uint32_t frames{};
  float averageMillis{};
  /// @brief The frame time 95 frames in 100 kept within: what the budget is held to, because an
  /// average hides the hitches a player feels and a maximum is one frame that may be the OS's.
  float p95Millis{};
  float maxMillis{};
  bool withinBudget{};
  /// @brief The costliest scopes, most first. See `StressScope`.
  std::vector<StressScope> scopes;
};
/// @brief Where a ramp has got to.
enum class StressPhase : uint8_t {
  /// @brief No ramp is running. A load set by hand may still be standing.
  Idle,
  /// @brief A step's load has just been put in, and frames are let go by unmeasured: the first
  /// frames after it pay for making it and for whatever the renderer builds the first time it sees it.
  Settling,
  Measuring
};
/// @brief A stress test: a load put into the active scene, and frame times measured under it.
///
/// Run from the command line with `--stress`, which ramps the named load up in `STRESS_STEPS` steps
/// once the scene is built, logs each step, writes `--stress-report` if given and quits; or from the
/// editor's Stress panel, through `SetLoad` and `BeginRun`. Not a rule and not part of any game: its
/// load is put in and measured in `Update`, and nothing it does reaches a tick.
///
/// A ramp turns vsync off while it runs, which would otherwise hold every frame to the display's
/// refresh and make a light load and a heavy one measure the same. The frame time measured is the
/// whole frame, from one frame's start to the next: it includes waiting for the GPU, so a load the
/// GPU cannot keep up with shows in it, though not which pass is to blame -- the scopes are the CPU's.
///
/// List it before `RenderingSystem`, and after `AnimationSystem` for its figures to move. Every
/// application that lists it is given its flags, and its assets: four meshes, eight materials and
/// the figure, under `MakeBuiltInAssetID("stress/...")` ids so a saved scene with a load in it opens.
class KUKI_ENGINE_API StressSystem final : public System {
public:
  static constexpr auto FLAGS = std::array{STRESS_FLAG, STRESS_SCALE_FLAG, STRESS_BUDGET_FLAG, STRESS_REPORT_FLAG};
  static constexpr uint32_t STRESS_STEPS = 4;
  static constexpr uint32_t SETTLE_FRAMES = 60;
  static constexpr uint32_t MEASURE_FRAMES = 240;
  /// @brief The most pools a load can be spread over: every mesh with every material.
  static constexpr uint32_t MAX_POOLS = 32;
  /// @brief Frames the live readout keeps, for a panel to plot. Two seconds at sixty a second.
  static constexpr size_t RECENT_FRAMES = 120;
  explicit StressSystem(Application &);
  auto Start() -> void override;
  auto PostStart() -> void override;
  auto Update(const float) -> void override;
  auto Shutdown() -> void override;
  /// @brief Replaces whatever load stands in the active scene with this one, placed in front of the
  /// camera. A load of nothing takes it away. Allocates: it makes and deletes entities.
  auto SetLoad(const StressLoad &) -> void;
  auto GetLoad() const -> const StressLoad &;
  auto Clear() -> void;
  /// @brief Ramps up to a load: `STRESS_STEPS` steps, a quarter of it more each, measuring each.
  /// Takes away the load when it ends, and puts back the vsync it found.
  auto BeginRun(const StressLoad &) -> void;
  /// @brief Stops a ramp where it is. The steps measured so far are kept.
  auto CancelRun() -> void;
  auto GetPhase() const -> StressPhase;
  /// @brief The step being run, from 0, and how many frames of its phase have gone by.
  auto GetStepIndex() const -> uint32_t;
  auto GetPhaseFrames() const -> uint32_t;
  auto GetSteps() const -> const std::vector<StressStep> &;
  auto GetBudget() const -> float;
  auto SetBudget(const float) -> void;
  /// @brief Whether the last step of the last ramp that finished kept within the budget.
  auto Passed() const -> bool;
  /// @brief The steps as JSON, with the budget they were held to. Whether it could be written.
  auto WriteReport(const std::filesystem::path &) const -> bool;
  /// @brief The last `RECENT_FRAMES` frames' times in milliseconds, oldest first from the offset.
  auto GetRecentFrames() const -> const std::array<float, RECENT_FRAMES> &;
  auto GetRecentOffset() const -> size_t;
private:
  auto LoadAssets() -> void;
  /// @brief Whether the scene has the entity a load hangs from, which is then `root`.
  ///
  /// Asked again each time rather than trusted: a scene opened or put back since (the editor's Stop)
  /// may have taken the load away, or brought it back under another id and given its old one to
  /// something else -- which a load deleted by id alone would have taken with it.
  auto HasRoot() -> bool;
  auto BeginStep() -> void;
  auto EndStep() -> void;
  auto EndRun() -> void;
  auto LogStep(const StressStep &) const -> void;
  StressLoad load{};
  StressLoad target{};
  EntityID root{};
  std::vector<AssetID> meshes;
  std::vector<AssetID> materials;
  AssetID figure{};
  StressPhase phase{StressPhase::Idle};
  uint32_t stepIndex{};
  uint32_t phaseFrames{};
  std::array<float, MEASURE_FRAMES> samples{};
  /// @brief Each profiler node's total when the step's measuring began, by node index: a node's
  /// share of the step is what it has gained since. Reserved at start; the profiler is never reset.
  std::vector<double> scopeTotals;
  std::vector<StressStep> steps;
  float budget{1000.f / 60.f};
  bool passed{};
  /// @brief What a ramp turned off or on, to put back when it ends.
  bool savedVSync{true};
  bool savedProfiling{true};
  bool savedPaused{};
  /// @brief Whether the ramp came from the command line, and so quits the application when it ends.
  bool fromCommandLine{};
  std::filesystem::path reportPath;
  std::array<float, RECENT_FRAMES> recent{};
  size_t recentOffset{};
  float clock{};
};
} // namespace kuki
