#pragma once
#include <engine_export.h>
#include <scene.hpp>
class ENGINE_EXPORT System {
public:
  virtual ~System() = default;
  virtual void Start() {};
  virtual void Update(float, Scene*) {};
  virtual void Shutdown() {};
};
