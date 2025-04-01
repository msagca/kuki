#pragma once
#include <application.hpp>
#include <camera_controller.hpp>
#include <imgui.h>
#include <imfilebrowser.h>
#include <unordered_set>
class Editor final : public Application {
private:
  CameraController cameraController;
  Camera editorCamera;
  ImGui::FileBrowser fileBrowser;
  std::unordered_set<unsigned int> selectedEntities;
  bool flying = false;
  bool addRangeToSelection = false;
  bool addToSelection = false;
  bool clearSelection = false;
  bool deleteSelected = false;
  int currentSelection = -1;
  int lastSelection = -1;
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
