#pragma once
#include <id.hpp>
#include <scene_manager.hpp>
#include <system.hpp>
#include <utility>
#include <vector>
namespace kuki {
class Script;
class KUKI_ENGINE_API ScriptingSystem final : public System {
public:
  ScriptingSystem(Application &);
  auto Start() -> void override;
  auto Update(const float) -> void override;
  auto Shutdown() -> void override;
private:
  auto StartIfIdle(Script *) -> void;
  /// @brief The scripts to run this frame, gathered before any of them runs.
  ///
  /// Scripts used to be called from inside `ForEachEntity<Script>`, and that was a trap with real
  /// teeth: `EntityManager` counts iteration depth, and while it is non-zero `AddComponent` defers
  /// the add and returns a null pointer. Every script's `Start` and `Update` ran at depth one, so a
  /// script could not build anything -- `AddEntityComponent<Transform>(id)` handed it null, the
  /// component appeared a moment later with nothing written into it, and the only symptom was
  /// geometry sitting at the origin at default scale.
  ///
  /// The depth counter is there to keep an archetype column from being reallocated while it is
  /// being walked. Script iteration never touches those columns -- `ForEach<Script>` walks
  /// `ScriptStore`, which is keyed by entity -- so the guard cost script code everything and
  /// protected it from nothing.
  ///
  /// Gathering first also makes the walk safe against a script that adds or removes a script, and
  /// the `IsEntity` check at each call covers a script that deletes an entity whose script is
  /// further down this list. A script created during the loop is not in it, so it starts on the
  /// next frame rather than half way through this one.
  auto Collect() const -> std::vector<std::pair<EntityID, Script *>>;
};
} // namespace kuki
