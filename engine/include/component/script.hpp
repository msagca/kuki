#pragma once
#include "component.hpp"
#include <kuki_engine_export.h>
namespace kuki {
class Application;
struct KUKI_ENGINE_API Script final : public IComponent {
  Script()
    : IComponent(std::in_place_type<Script>) {}
  int id{};
};
class KUKI_ENGINE_API IScript {
protected:
  Application& app;
  unsigned int entityId;
public:
  IScript(Application&, unsigned int);
  virtual ~IScript() = default;
  virtual void Update(float) = 0;
};
} // namespace kuki
