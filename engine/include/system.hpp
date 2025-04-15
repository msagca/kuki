#pragma once
#include <kuki_export.h>
#include <scene.hpp>
class Application;
class KUKI_API System {
protected:
  Application& app;
public:
  System(Application&);
  virtual ~System() = default;
  virtual void Start() {};
  virtual void Update(double, Scene*) {};
  virtual void Shutdown() {};
};
