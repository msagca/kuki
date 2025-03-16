#pragma once
#include <engine_export.h>
#include <scene.hpp>
class Application;
class ENGINE_API System {
protected:
  Application& app;
public:
  System(Application&);
  virtual ~System() = default;
  virtual void Start() {};
  virtual void Update(float, Scene*) {};
  virtual void Shutdown() {};
};
