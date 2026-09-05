#pragma once
#include <animation_system.hpp>
#include <application.hpp>
#include <camera_controller.hpp>
#include <cstdint>
#include <engine_config.hpp>
#include <filesystem>
#include <glm/ext/vector_float2.hpp>
#include <id.hpp>
#include <imgui.h>
#include <imgui_file_browser.hpp>
#include <material_type.hpp>
#include <physics_system.hpp>
#include <renderer_capabilities.hpp>
#include <rendering_api.hpp>
#include <rendering_system.hpp>
#include <scripting_system.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <system_application.hpp>
#include <unordered_set>
using namespace kuki;
inline constexpr ImU32 PICKING_HIGHLIGHT_COLOR = IM_COL32(255, 0, 200, 255);
inline constexpr float PICKING_HIGHLIGHT_THICKNESS = 3.0f;
/// @brief Draws an image button, falling back to a plain button when there is no texture to show.
///
/// A zero handle is not the harmless "draw nothing" it is under OpenGL. Dear ImGui records it
/// verbatim and the Direct3D 12 backend feeds it straight to `SetGraphicsRootDescriptorTable`, so a
/// null GPU descriptor handle reaches the driver and removes the device. Unassigned texture slots
/// and backends that cannot preview an asset both produce zero, so every image the editor draws
/// goes through here rather than calling `ImGui::ImageButton` directly.
///
/// The fallback keeps the same widget id and footprint, so layout and picking are unaffected.
inline auto TextureButton(const char *id, const ImTextureID tex, const ImVec2 &size, const ImVec2 &uv0 = ImVec2(.0f, .0f), const ImVec2 &uv1 = ImVec2(1.f, 1.f)) -> bool {
  if (tex == ImTextureID_Invalid) {
    const auto &style = ImGui::GetStyle();
    return ImGui::Button(id, ImVec2(size.x + style.FramePadding.x * 2.f, size.y + style.FramePadding.y * 2.f));
  }
  return ImGui::ImageButton(id, tex, size, uv0, uv1);
}
/// @brief Draws an image, reserving the space but drawing nothing when there is no texture.
inline auto TextureImage(const ImTextureID tex, const ImVec2 &size, const ImVec2 &uv0 = ImVec2(.0f, .0f), const ImVec2 &uv1 = ImVec2(1.f, 1.f)) -> void {
  if (tex == ImTextureID_Invalid) {
    ImGui::Dummy(size);
    return;
  }
  ImGui::Image(tex, size, uv0, uv1);
}
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
  kuki::RenderingAPI pendingApi{kuki::DefaultAPI()};
  /// @brief What the running backend can do, so no panel offers a control that reaches nothing.
  ///
  /// Read once in `Editor::Start` rather than every frame. A backend cannot be exchanged without
  /// restarting the process -- which is what the graphics panel says when a different one is picked
  /// -- so there is nothing here that can change while the editor is up.
  kuki::RendererCapabilities capabilities{};
};
class Editor final : public SystemApplication<ScriptingSystem, AnimationSystem, PhysicsSystem, RenderingSystem> {
public:
  Editor();
  EditorContext context{};
  /// @brief The editor's own tooling, which used to sit on the engine's `Application` facade.
  ///
  /// Every one of these is a question only an editor asks. Thumbnails at a size a panel picked; a
  /// frame counter for a status bar; a render resolution that follows a dockable viewport rather
  /// than the window; which components an entity is missing, which is the Add Component menu's
  /// question and nobody else's; removing a script by `type_index`, which is type-erased only
  /// because an inspector row has no static type to hand; and the rebinding panel's key capture.
  ///
  /// They were public on `Application`, so `Script::Update` handed all of them to every script
  /// along with the rest of the facade. Here instead, because `Editor` is what they belong to and
  /// a script holding an `Application &` cannot see them. The engine library stops exporting an
  /// editor's vocabulary, and the two widgets below that need them take an `Editor &` and get them.
  ///
  /// Public rather than private because `PropertyDisplayer` and `DisplayKeyBindingRow` are not
  /// members and do need them; the rest of `Editor` stays private as it was.
  auto GetFPS() -> size_t;
  auto GetPreviewSize() -> int;
  auto PreviewAsset(const AssetID) -> RenderTarget *;
  auto SetPreviewSize(const int) -> void;
  auto SetResolution(const int = 1920, const int = 1080) -> void;
  auto GetMissingEntityComponents(const EntityID) const -> std::vector<ComponentType>;
  /// @brief Whether the scene already holds this component on any entity.
  ///
  /// What the editor asks before offering a `Component::IsSceneSingleton` type in the Add Component
  /// menu, so a second one is never offered rather than being added and then disputed.
  auto HasComponentAnywhere(const ComponentType) const -> bool;
  auto RemoveEntityScript(const EntityID, const std::type_index) -> bool;
  auto BeginKeyCapture() -> void;
  auto PollKeyCapture() -> InputManager::CaptureOutcome;
  auto GetBinding(const std::string &) -> InputManager::Trigger;
  auto SetBinding(const std::string &, const InputManager::Trigger &) -> void;
  auto GetTriggerName(const InputManager::Trigger &) -> std::string;
  auto GetBindingNames() -> const std::vector<std::string> &;
  auto GetBindingDescription(const std::string &) -> std::string;
  auto GetSequenceNames() -> const std::vector<std::string> &;
  auto GetSequenceDescription(const std::string &) -> std::string;
  /// @brief Puts the editor into and out of the state where a keystroke is a binding, not a shortcut.
  ///
  /// No longer an override. `Application` carried this as an empty virtual purely so the rebinding
  /// widget could reach it through a base reference, which is a hook shaped by its one caller; the
  /// widget takes an `Editor &` now and calls it directly.
  auto SetInputCaptureActive(bool) -> void;
private:
  ImGui::FileBrowser fileBrowser{};
  ImGui::FileBrowser sceneFileBrowser{ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_ConfirmOnEnter};
  SceneFileAction sceneFileAction{SceneFileAction::Save};
  std::vector<EntityID> displayedEntities{};
  std::vector<EntityID> displayedEntitiesNext{};
  int shortcutRebindingIndex{-1};
  glm::vec2 pickPressPos{};
  bool trackingClick{false};
  bool mouselookActive{false};
  bool entityContextMenuTriggered{false};
  EntityManager entityClipboard{};
  std::vector<std::pair<EntityID, EntityID>> entityClipboardRoots{};
  auto Start() -> void override;
  auto Update(const float) -> void override;
  auto Shutdown() -> void override;
  auto GetSelectedEntity() const -> EntityID override;
  auto GetSelectedEntities() const -> std::vector<EntityID> override;
  auto InitImGui() -> void;
  auto LoadDefaultScene() -> void;
  /// @brief Writes the whole graphics section back out, rather than the one field that changed.
  ///
  /// `EngineConfig::Save` writes every field it holds, so saving a freshly constructed one to record
  /// a single change would reset the others to their defaults. Reading first and overwriting only
  /// what the panel owns keeps a setting this build does not display from being dropped by a build
  /// that does not know about it.
  auto SaveGraphicsConfig() -> void;
  auto UpdateIO() -> void;
  auto UpdateView() -> void;
  auto ApplyPickedAsset(const AssetID) -> void;
  auto AttachCameraController() -> EntityID;
  /// @brief Puts the settings that are components onto an entity, so there is somewhere to find them.
  ///
  /// None of these are serialized, so every scene arrives without them and every scene needs them
  /// put back. Called wherever `AttachCameraController` is, and for the same reason: it is the other
  /// thing a freshly loaded scene has no way to have brought with it.
  auto AttachSettingsEntity() -> EntityID;
  /// @brief Whether a singleton component is already somewhere in the scene, so it is not offered again.
  auto HasSceneSingleton(const kuki::ComponentType) const -> bool;
  auto CopySelectedEntities() -> void;
  auto PasteEntities() -> void;
  auto DisplayAnimation() -> void;
  auto DisplayAssetBrowser(const float) -> void;
  auto DisplayAssets() -> void;
  auto DisplayEntity(const EntityID) -> void;
  auto DisplayHierarchy() -> void;
  auto DisplayPanelBar() -> void;
  auto DisplayProfileNode(const size_t) -> void;
  auto DisplayProfiler() -> void;
  auto DisplayProperties() -> void;
  auto DisplayProperties(const ComponentVariant &) -> void;
  auto DisplayScene() -> void;
  auto DisplaySettings() -> void;
  auto DrawManipulator(const float, const float) -> void;
  auto LoadScene(const std::filesystem::path &) -> void;
  auto SaveScene(const std::filesystem::path &) -> void;
};
