#pragma once
#include <application.hpp>
#include <camera_controller.hpp>
#include <component.hpp>
#include <component_traits.hpp>
#include <id.hpp>
#include <imgui.h>
#include <imgui_sink.hpp>
#include <limits>
#include <mutex>
#include <rendering_system.hpp>
#include <spdlog/spdlog.h>
#include <unordered_set>
//
#include <imfilebrowser.h>
using namespace kuki;
/// @brief A container for the editor state
struct EditorContext {
  size_t assetMask{std::numeric_limits<size_t>::max()};
  size_t gizmoMask{0};
  /// @brief Stores the last clicked entity ID, acts as an anchor in multi-selection
  ID selectedEntity{ID::Invalid()};
  int selectedComponent{-1};
  int selectedProperty{-1};
};
class Editor final : public Application {
private:
  EditorContext context{};
  ImGui::FileBrowser fileBrowser{};
  std::shared_ptr<ImGuiSink<std::mutex>> imguiSink{};
  std::shared_ptr<spdlog::logger> logger{};
  /// @brief A set containing the IDs of all selected entities
  std::unordered_set<ID> selection{};
  std::vector<ID> displayedEntities{};
  std::unique_ptr<CameraController> cameraController{};
  bool showConsole{false};
  bool showFPS{true};
  bool ctrlHeld{false};
  bool shiftHeld{false};
  bool backspacePressed{false};
  bool deletePressed{false};
  bool enterPressed{false};
  bool escapePressed{false};
  bool spacePressed{false};
  void DisplayAssets();
  void DisplayComponents();
  void DisplayEntity(ID);
  void DisplayHierarchy();
  void DisplayLogs();
  void DisplayProperties(IComponent*);
  void DisplayScene();
  void DrawManipulator(float, float);
  void InitImGui();
  void InitLayout();
  void LoadDefaultAssets();
  void LoadDefaultScene();
  void ToggleGizmo(GizmoType);
  ComponentType GetComponentType(IComponent*);
  std::string GetComponentName(IComponent*);
  void Init() override;
  void Start() override;
  void Update() override;
  void LateUpdate() override;
  void UpdateView();
  void Shutdown() override;
public:
  Editor();
  ~Editor();
  ID Instantiate(std::string&, const ID = ID::Invalid(), glm::vec3 = glm::vec3{}, glm::quat = glm::quat{}, glm::vec3 = glm::vec3{1.f});
  /// @brief Create multiple instances of the specified asset at random positions
  /// @param name Name string of the asset
  /// @param count Number of instances to create
  /// @param radius Radius of the spherical volume that is the spawn region
  void InstantiateRandom(const std::string&, size_t, float);
};
class SpawnCommand final : public ICommand {
private:
  Editor& app;
public:
  SpawnCommand(Editor&);
  std::string GetMessage(int) override;
  int Execute(const std::span<std::string>) override;
};
class DeleteCommand final : public ICommand {
private:
  Editor& app;
public:
  DeleteCommand(Editor&);
  std::string GetMessage(int) override;
  int Execute(const std::span<std::string>) override;
};
