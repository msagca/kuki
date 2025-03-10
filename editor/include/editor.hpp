#pragma once
#include <application.hpp>
#include <camera_controller.hpp>
#include <imgui.h>
#include <imfilebrowser.h>
#include <utility/pool.hpp>
class Editor : public Application {
private:
  CameraController cameraController;
  ImGui::FileBrowser fileBrowser;
  Pool<unsigned int> texturePool;
  int clickedEntity = -1;
  int selectedEntity = -1;
  int entityToDelete = -1;
  bool updateThumbnails = true;
  static unsigned int CreateTexture();
  static void DeleteTexture(unsigned int);
  void DisplayEntity(unsigned int);
  void DisplayHierarchy();
  void DisplayProperties();
  void DisplayAssets();
  void DisplayScene();
  /// <summary>
  /// Draw gizmos in ImGui space; they appear either in front of or behind everything in the scene, depending on the draw order
  /// </summary>
  void DrawGizmos();
  void InitImGui();
  void InitLayout();
  void LoadDefaultAssets();
  void LoadDefaultScene();
  void Shutdown() override;
  void Start() override;
  void Update() override;
  void UpdateThumbnails();
  void UpdateView();
public:
  Editor();
};
