#pragma once
#include <application.hpp>
#include <camera_controller.hpp>
#include <imgui.h>
#include <imfilebrowser.h>
#include <unordered_set>
class Editor final : public Application {
private:
  CameraController cameraController;
  ImGui::FileBrowser fileBrowser;
  int lastSelection = -1;
  int currentSelection = -1;
  void DisplayEntity(unsigned int);
  void DisplayHierarchy();
  void DisplayProperties();
  void DisplayAssets();
  void DisplayScene();
  void DisplayConsole();
  void DrawGizmos(float, float);
  void InitImGui();
  void InitLayout();
  void LoadDefaultAssets();
  void LoadDefaultScene();
  void Shutdown() override;
  void Start() override;
  void Update() override;
  void UpdateView();
public:
  Editor();
};
