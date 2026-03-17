#pragma once
#include <application.hpp>
class Game final : public kuki::Application {
public:
  Game();
  ~Game();
  void Awake() override;
  void Start() override;
  void Update(const float) override;
  void Shutdown() override;
  bool Status() override;
};
