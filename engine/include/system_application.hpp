#pragma once
#include <application.hpp>
#include <application_description.hpp>
#include <array>
#include <concepts.hpp>
#include <rendering_system.hpp>
#include <scripting_system.hpp>
#include <system.hpp>
#include <tuple>
#include <type_traits>
#include <utility>
namespace kuki {
template <IsSystem... Systems>
class SystemApplication : public Application {
public:
  explicit SystemApplication(ApplicationDescription desc = {})
    : Application(std::move(desc)), systemsTuple(Systems(*this)...), systems{&std::get<Systems>(systemsTuple)...} {}
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
    std::apply([](auto &...system) { (StartUnlessScripting(system), ...); }, systemsTuple);
  }
  auto PostStartSystems() -> void override {
    std::apply([](auto &...system) { (StartIfScripting(system), ...); }, systemsTuple);
  }
  auto UpdateSystems(const float deltaTime) -> void override {
    for (auto *system : systems)
      system->Update(deltaTime);
  }
  auto ShutdownSystems() -> void override {
    for (auto *system : systems)
      system->Shutdown();
  }
private:
  std::tuple<Systems...> systemsTuple;
  std::array<System *, sizeof...(Systems)> systems;
  template <typename T>
  static auto StartUnlessScripting(T &system) -> void {
    if constexpr (!std::is_same_v<T, ScriptingSystem>)
      system.Start();
  }
  template <typename T>
  static auto StartIfScripting(T &system) -> void {
    if constexpr (std::is_same_v<T, ScriptingSystem>)
      system.Start();
  }
};
} // namespace kuki
