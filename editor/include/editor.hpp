#pragma once
#include <animation_system.hpp>
#include <application.hpp>
#include <camera_controller.hpp>
#include <cstdint>
#include <filesystem>
#include <glm/ext/vector_float2.hpp>
#include <id.hpp>
#include <imgui_file_browser.hpp>
#include <material_type.hpp>
#include <physics_system.hpp>
#include <rendering_system.hpp>
#include <scripting_system.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <system_application.hpp>
#include <unordered_set>
using namespace kuki;
inline constexpr ImU32 PICKING_HIGHLIGHT_COLOR = IM_COL32(255, 0, 200, 255);
inline constexpr float PICKING_HIGHLIGHT_THICKNESS = 3.0f;
enum class EditorState : uint8_t {
  Normal,
  Rename,
  PickingAsset,
  RebindingKey
};
enum class PickingTarget : uint8_t {
  MaterialTexture,
  Skybox
};
enum class SceneFileAction : uint8_t {
  Save,
  Load
};
enum class AssetViewMode : uint8_t {
  List,
  Thumbnail
};
enum class KeyBit : uint8_t {
  Alt,
  Backspace,
  Ctrl,
  Delete,
  Enter,
  Escape,
  Shift,
  Space
};
using KeyMask = std::bitset<static_cast<uint8_t>(KeyBit::Space) + 1>;
struct EditorContext {
  AssetViewMode assetViewMode{AssetViewMode::List};
  std::string debugViewTarget{};
  EditorState state{EditorState::Normal};
  EntityID cameraEntity{};
  EntityID renamedEntityId{};
  EntityID selectedEntityId;
  KeyMask keyState{};
  KeyMask pressState{};
  KeyMask releaseState{};
  bool showFPS{};
  std::unordered_set<EntityID> selectedEntities{};
  bool selectionActive{false};
  ImVec2 selectionStart{};
  ImVec2 selectionEnd{};
  AssetType pickingAssetType{};
  EntityID pickingEntityId{};
  MaterialProperty pickingProperty{};
  PickingTarget pickingTarget{PickingTarget::MaterialTexture};
};
class Editor final : public SystemApplication<ScriptingSystem, AnimationSystem, PhysicsSystem, RenderingSystem> {
public:
  Editor();
  EditorContext context{};
private:
  ImGui::FileBrowser fileBrowser{};
  ImGui::FileBrowser sceneFileBrowser{ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_ConfirmOnEnter};
  SceneFileAction sceneFileAction{SceneFileAction::Save};
  std::vector<EntityID> displayedEntities{};
  std::vector<EntityID> displayedEntitiesNext{};
  int shortcutRebindingIndex{-1};
  glm::vec2 pickPressPos{};
  bool trackingClick{false};
  bool entityContextMenuTriggered{false};
  EntityManager entityClipboard{};
  std::vector<std::pair<EntityID, EntityID>> entityClipboardRoots{};
  auto Start() -> void override;
  auto Update(const float) -> void override;
  auto Shutdown() -> void override;
  auto GetSelectedEntity() const -> EntityID override;
  auto GetSelectedEntities() const -> std::vector<EntityID> override;
  auto SetInputCaptureActive(bool) -> void override;
  auto InitImGui() -> void;
  auto InitLayout() -> void;
  auto LoadDefaultScene() -> void;
  auto UpdateIO() -> void;
  auto UpdateView() -> void;
  auto ApplyPickedAsset(const AssetID) -> void;
  auto AttachCameraController() -> EntityID;
  auto CopySelectedEntities() -> void;
  auto PasteEntities() -> void;
  auto DisplayAnimation() -> void;
  auto DisplayAssetBrowser(const float) -> void;
  auto DisplayAssets() -> void;
  auto DisplayEntity(const EntityID) -> void;
  auto DisplayHierarchy() -> void;
  auto DisplayProperties() -> void;
  auto DisplayProperties(const ComponentVariant &) -> void;
  auto DisplayScene() -> void;
  auto DisplaySettings() -> void;
  auto DrawManipulator(const float, const float) -> void;
  auto LoadScene(const std::filesystem::path &) -> void;
  auto SaveScene(const std::filesystem::path &) -> void;
};
