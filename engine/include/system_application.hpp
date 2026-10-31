#pragma once
#include <application.hpp>
#include <application_description.hpp>
#include <array>
#include <command_line.hpp>
#include <concepts.hpp>
#include <cstdlib>
#include <rendering_system.hpp>
#include <spdlog/spdlog.h>
#include <system.hpp>
#include <tuple>
#include <type_traits>
#include <utility>
#include <world.hpp>
namespace kuki {
/// @brief An application made of a world and a list of systems, both fixed at compile time.
///
/// `W` is the game's `World`: the archetypes every scene is given, and so everything its entities
/// can be made of. `EngineWorld` for an application that adds none.
///
/// The systems run in the order listed, in each phase: every `Start`, then the application's own
/// `Start` (where a game builds its scene), then every `PostStart`; each tick every `Tick`; each
/// frame every `Update`, every `LateUpdate`, and the drawing.
template <IsWorld W, IsSystem... Systems>
class SystemApplication : public Application {
public:
  using WorldType = W;
  /// @brief Every command-line flag the application reads: the engine's and the editor's, and those
  /// of each system that declares any. See `Flag`.
  static constexpr auto FLAGS = JoinFlags(ENGINE_FLAGS, FlagsOf<Systems>()...);
  static_assert(HasDistinctNames(FLAGS), "two of this application's systems, or a system and the engine or the editor, read a command-line flag of the same name -- see each system's FLAGS");
  explicit SystemApplication(ApplicationDescription desc = {})
    : Application(std::move(desc)), systemsTuple(Systems(*this)...), systems{&std::get<Systems>(systemsTuple)...} {
    SetArchetypes(W::Specs());
    CheckArguments();
  }
  template <IsSystem T>
  auto GetSystem(this auto &self) -> decltype(auto) {
    return &std::get<T>(self.systemsTuple);
  }
protected:
  auto GetRenderingSystem() -> RenderingSystem * override {
    static_assert((std::is_same_v<Systems, RenderingSystem> || ...), "SystemApplication<...> must include RenderingSystem -- Application's own startup/render-pass logic depends on it existing");
    return &std::get<RenderingSystem>(systemsTuple);
  }
  auto StartSystems() -> void override {
    for (auto *system : systems)
      system->Start();
  }
  auto PostStartSystems() -> void override {
    for (auto *system : systems)
      system->PostStart();
  }
  /// @brief Every system's `Update`, then every `LateUpdate`, then the drawing.
  ///
  /// Drawing is `RenderingSystem::Update`, and it is held back to the end wherever the system stands
  /// in the list, so that a late update is never too late to be seen. The list puts it last anyway.
  /// The settings menu comes just before it, so that it is laid over everything the systems drew.
  auto UpdateSystems(const float deltaTime) -> void override {
    auto *rendering = static_cast<System *>(GetRenderingSystem());
    for (auto *system : systems)
      if (system != rendering)
        system->Update(deltaTime);
    for (auto *system : systems)
      system->LateUpdate(deltaTime);
    UpdateSettingsMenu(deltaTime);
    rendering->Update(deltaTime);
  }
  auto TickSystems() -> void override {
    for (auto *system : systems)
      system->Tick();
  }
  auto ShutdownSystems() -> void override {
    for (auto *system : systems)
      system->Shutdown();
  }
private:
  std::tuple<Systems...> systemsTuple;
  /// @brief Answers `--help`, and says which arguments nothing in the application reads.
  ///
  /// A warning and not a refusal: an editor hosting a game is given the game's flags too, which it
  /// cannot know. In a game's own executable it is the typo it looks like.
  static auto CheckArguments() -> void {
    const auto arguments = GetCommandLineArguments();
    if (HasFlag(arguments, HELP_FLAG)) {
      spdlog::info("Flags:\n{}", DescribeFlags(FLAGS));
      std::exit(0);
    }
    for (const auto &unknown : FindUnknownFlags(arguments, FLAGS))
      spdlog::warn("[Application] Nothing reads the flag {}; --help lists those that something does", unknown);
  }
  std::array<System *, sizeof...(Systems)> systems;
};
} // namespace kuki
