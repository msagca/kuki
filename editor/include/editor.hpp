#pragma once
#include <application.hpp>
#include <component/component.hpp>
#include <camera_controller.hpp>
#include <imgui.h>
#include <imgui_sink.hpp>
#include <mutex>
#include <spdlog/spdlog.h>
/**/
#include <imfilebrowser.h>
class Editor final : public kuki::Application {
private:
  ImGui::FileBrowser fileBrowser;
  std::shared_ptr<ImGuiSink<std::mutex>> imguiSink;
  std::shared_ptr<spdlog::logger> logger;
  ImGuiSelectionBasicStorage selection{};
  int selectedEntity{-1};
  std::string selectedComponentName{};
  kuki::Property selectedProperty;
  int assetMask{-1}; // for filtering assets by type
  int gizmoMask{0};
  bool displayFPS{true};
  std::unique_ptr<CameraController> cameraController;
  void DisplayAssets();
  void DisplayConsole();
  void DisplayEntity(unsigned int);
  void DisplayHierarchy();
  void DisplayLogs();
  void DisplayProperties();
  void DisplayScene();
  void DrawGizmos(float, float, unsigned int);
  void ToggleFPS();
  void ToggleGizmos();
  std::vector<unsigned int> GetSelectedEntityIds();
  void RemoveDeletedEntitiesFromSelection();
  void InitImGui();
  void InitLayout();
  void LoadDefaultAssets();
  void LoadDefaultScene();
  void Init() override;
  void Start() override;
  void Update() override;
  void UpdateView();
  void Shutdown() override;
public:
  Editor();
};
