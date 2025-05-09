#pragma once
#include <kuki_engine_export.h>
#include <scene.hpp>
namespace kuki {
class Application;
class KUKI_ENGINE_API System {
protected:
  Application& app;
public:
  System(Application&);
  virtual ~System() = default;
  virtual void Start() {};
  virtual void Update(float) {};
  virtual void Shutdown() {};
};
} // namespace kuki
