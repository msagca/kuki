#pragma once
class ISystem {
public:
  virtual ~ISystem() = default;
  virtual void Update() = 0;
  virtual void CleanUp() = 0;
};
