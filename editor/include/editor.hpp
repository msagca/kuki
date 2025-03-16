#pragma once
#include <application.hpp>
#include <camera_controller.hpp>
#include <imgui.h>
#include <imfilebrowser.h>
class Editor : public Application {
private:
  CameraController cameraController;
  ImGui::FileBrowser fileBrowser;
  int clickedEntity = -1;
  int selectedEntity = -1;
  int entityToDelete = -1;
  void DisplayEntity(unsigned int);
  void DisplayHierarchy();
  void DisplayProperties();
  void DisplayAssets();
  void DisplayScene();
  void DisplayConsole();
  /// <summary>
  /// Draw gizmos in ImGui space; they appear either in front of or behind everything in the scene, depending on the draw order
  /// </summary>
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
