#pragma once
#include <application.hpp>
class Game final : public kuki::Application {
public:
  Game();
  void Start() override;
  void Update(const float) override;
  void Shutdown() override;
};
