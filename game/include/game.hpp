#pragma once
#include <application.hpp>
class Game : public Application {
public:
  void Start() override;
  bool Status() override;
  void Update() override;
  void Shutdown() override;
};
