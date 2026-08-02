#pragma once
#include <scene_manager.hpp>
#include <system.hpp>
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
};
} // namespace kuki
