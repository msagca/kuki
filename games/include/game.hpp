#pragma once
#include <application.hpp>
using namespace kuki;
class Game final : public Application {
public:
  Game();
  ~Game();
  void Start() override;
  bool Status() override;
  void Update() override;
  void LateUpdate() override;
  void Shutdown() override;
};
