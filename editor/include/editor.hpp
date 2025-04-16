#pragma once
#include <application.hpp>
#include <camera_controller.hpp>
#include <imgui.h>
#include <imgui_sink.hpp>
#include <mutex>
#include <spdlog/spdlog.h>
/**/
#include <imfilebrowser.h>
class Editor final : public kuki::Application {
private:
  CameraController cameraController;
  kuki::Camera editorCamera;
  ImGui::FileBrowser fileBrowser;
  std::shared_ptr<ImGuiSink<std::mutex>> imguiSink;
  std::shared_ptr<spdlog::logger> logger;
  ImGuiSelectionBasicStorage selection;
  int selectedEntity{-1};
  void DisplayAssets();
  void DisplayConsole();
  void DisplayEntity(unsigned int, std::vector<unsigned int>&, const ImGuiSelectionBasicStorage&);
  void DisplayHierarchy();
  void DisplayLogs();
  void DisplayProperties();
  void DisplayScene();
  void DrawGizmos(float, float, unsigned int);
  void EntityCreatedCallback(const kuki::EntityCreatedEvent&);
  void EntityDeletedCallback(const kuki::EntityDeletedEvent&);
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
