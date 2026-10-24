#pragma once
#include <concepts.hpp>
#include <kuki_engine_export.h>
#include <scene.hpp>
#include <typeindex>
namespace kuki {
class Application;
class KUKI_ENGINE_API System {
public:
  virtual ~System() = default;
  /// @brief Before the application's own `Start`, which is where a game loads its assets and builds
  /// its scene. For what the rest is built on: the renderer, the animation system.
  virtual auto Start() -> void {};
  /// @brief After the application's own `Start`, once the game has built its scene. Called for every
  /// system in order.
  ///
  /// Where a game's own system begins: it finds the assets loaded and the scene built, and can place
  /// what it plays with. What `ScriptingSystem` was deferred to this point to do for scripts, every
  /// system can now do for itself.
  virtual auto PostStart() -> void {};
  virtual auto Update(const float) -> void {};
  /// @brief One fixed step of the game's rules, as against `Update`, which is one frame of whatever length.
  ///
  /// Called for every system in order, once for each tick a frame owes and before that frame's
  /// `Update`, and always standing for `Application::TickTime` seconds. Unlike `Update` it is told
  /// no duration, because there is only one it could be. What decides how a match goes belongs here:
  /// movement, timing, scoring, and reading the player, through `Application::GetButtons` rather
  /// than the key queries, which answer for the frame and not the tick. What only decides how it
  /// looks belongs in `Update`, which can use `Application::TickAlpha` to draw between two ticks.
  ///
  /// The reason to keep the two apart is that a tick given the same state and the same buttons has
  /// to leave the same state, on any machine at any frame rate. Reading `DeltaTime`, the clock, the
  /// mouse or an unseeded random number in here is how that stops being true.
  virtual auto Tick() -> void {};
  /// @brief After every system's `Update` and before anything is drawn.
  ///
  /// For what has to see the frame as the others have left it: where a game's systems have put
  /// things, especially, which is where the animation system's goals are reached for.
  virtual auto LateUpdate(const float) -> void {};
  virtual auto Shutdown() -> void {};
protected:
  template <typename T>
  explicit System(std::in_place_type_t<T>, Application &);
  template <IsSystem T>
  auto As(this auto &self) -> ConstCorrectPointer<decltype(self), T>;
  template <IsSystem T>
  auto Is() const -> bool;
  Application &app;
private:
  std::type_index typeIndex;
};
template <typename T>
System::System(std::in_place_type_t<T>, Application &app)
  : typeIndex(typeid(T)), app(app) {}
template <IsSystem T>
auto System::As(this auto &self) -> ConstCorrectPointer<decltype(self), T> {
  if (self.template Is<T>())
    return &self;
  return nullptr;
}
template <IsSystem T>
auto System::Is() const -> bool {
  return typeIndex == typeid(T);
}
} // namespace kuki
