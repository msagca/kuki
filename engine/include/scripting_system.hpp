#pragma once
#include <scene_manager.hpp>
#include <system.hpp>
namespace kuki {
class KUKI_ENGINE_API ScriptingSystem final : public System {
public:
  ScriptingSystem(Application &);
  ~ScriptingSystem();
  auto Start() -> void override;
  auto Update(const float) -> void override;
  auto Shutdown() -> void override;
private:
  Application &app;
};
} // namespace kuki
