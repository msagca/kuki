#pragma once
#include "component/component_traits.hpp"
#include "system/rendering.hpp"
#include <application.hpp>
#include <camera_controller.hpp>
#include <component/component.hpp>
#include <imgui.h>
#include <imgui_sink.hpp>
#include <mutex>
#include <spdlog/spdlog.h>
/**/
#include <imfilebrowser.h>
using namespace kuki;
/// @brief A container for the editor state
struct EditorContext {
  bool displayFPS{true};
  int assetMask{-1};
  int gizmoMask{0};
  int selectedEntity{-1};
  int selectedComponent{-1};
  int selectedProperty{-1};
};
class Editor final : public Application {
private:
  EditorContext context{};
  ImGui::FileBrowser fileBrowser{};
  std::shared_ptr<ImGuiSink<std::mutex>> imguiSink{};
  std::shared_ptr<spdlog::logger> logger{};
  ImGuiSelectionBasicStorage selection{};
  std::unique_ptr<CameraController> cameraController{};
  ComponentType GetComponentType(IComponent*);
  std::string GetComponentName(IComponent*);
  std::vector<unsigned int> GetSelectedEntityIds();
  void DisplayAssets();
  void DisplayComponents();
  void DisplayConsole();
  void DisplayEntity(unsigned int);
  void DisplayGraphEditor();
  void DisplayHierarchy();
  void DisplayLogs();
  void DisplayProperties(IComponent*);
  void DisplayScene();
  void DrawGizmos(float, float, unsigned int);
  void InitImGui();
  void InitLayout();
  void LoadDefaultAssets();
  void LoadDefaultScene();
  void RemoveDeletedEntitiesFromSelection();
  void ToggleFPS();
  void ToggleGizmo(GizmoType);
  void Init() override;
  void Start() override;
  void Update() override;
  void UpdateView();
  void Shutdown() override;
public:
  Editor();
};
