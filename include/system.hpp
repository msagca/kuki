#pragma once
class ISystem {
public:
  virtual ~ISystem() = default;
  virtual void Update() = 0;
};
