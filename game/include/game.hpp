#pragma once
#include <application.hpp>
class Game : public Application {
private:
  bool standalone;
  unsigned int renderTexture;
  void RenderToTexture();
  void RenderToWindow();
public:
  void Start() override;
  bool Status() override;
  void Update() override;
  void Shutdown() override;
};
