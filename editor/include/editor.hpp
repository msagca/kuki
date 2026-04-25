#pragma once
#include <application.hpp>
#include <camera_controller.hpp>
#include <cstdint>
#include <id.hpp>
#include <imgui.h>
#include <rendering_system.hpp>
#include <spdlog/spdlog.h>
#include <unordered_set>
//
#include <imfilebrowser.h>
using namespace kuki;
enum class EditorState : uint8_t {
  Normal,
  Rename
};
enum class KeyBit : uint8_t {
  Alt,
  Backspace,
  Ctrl,
  Delete,
  Enter,
  Escape,
  F,
  Shift,
  Space,
  V
};
using KeyMask = std::bitset<static_cast<uint8_t>(KeyBit::V) + 1>;
struct EditorContext {
  AssetType selectedAssetType{AssetType::Texture};
  EditorState state{EditorState::Normal};
  EntityID cameraEntity{};
  EntityID renamedEntityId{};
  EntityID selectedEntityId;
  KeyMask keyState{};
  KeyMask pressState{};
  KeyMask releaseState{};
  bool showFPS{};
  std::unordered_set<EntityID> selectedEntities{};
};
class Editor final : public Application {
public:
  Editor();
  EditorContext context{};
private:
  ImGui::FileBrowser fileBrowser{};
  std::vector<EntityID> displayedEntities{};
  auto Start() -> void override;
  auto Update(const float) -> void override;
  auto Shutdown() -> void override;
  auto InitImGui() -> void;
  auto InitLayout() -> void;
  auto LoadDefaultAssets() -> void;
  auto LoadDefaultScene() -> void;
  auto UpdateIO() -> void;
  auto UpdateView() -> void;
  auto DisplayAssetCategories() -> void;
  auto DisplayAssets() -> void;
  auto DisplayEntity(const EntityID) -> void;
  auto DisplayHierarchy() -> void;
  auto DisplayProperties() -> void;
  auto DisplayProperties(const ComponentVariant &) -> void;
  auto DisplayScene() -> void;
  auto DrawManipulator(const float, const float) -> void;
};
