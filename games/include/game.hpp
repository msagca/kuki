#pragma once
#include <application.hpp>
class Game final : public kuki::Application {
public:
  Game();
  void Start() override;
  bool Status() override;
  void Update() override;
  void Shutdown() override;
};
