#include <GLFW/glfw3.h>
#include <algorithm>
#include <animator.hpp>
#include <application.hpp>
#include <application_description.hpp>
#include <array>
#include <asset.hpp>
#include <asset_type.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <camera_controller.hpp>
#include <camera_type.hpp>
#include <component.hpp>
#include <component_reflection.hpp>
#include <cstdint>
#include <editor.hpp>
#include <filesystem>
#include <gl_render_target.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <id.hpp>
#include <imgui_file_browser.hpp>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <imguizmo.hpp>
#include <key_binding_widget.hpp>
#include <light.hpp>
#include <material_type.hpp>
#include <model_asset.hpp>
#include <property_displayer.hpp>
#include <rendering_system.hpp>
#include <scene_serializer.hpp>
#include <script_registry.hpp>
#include <shader_asset.hpp>
#include <skeleton.hpp>
#include <skybox_handle.hpp>
#include <spdlog/spdlog.h>
#include <string.h>
#include <string>
#include <texture_asset.hpp>
#include <texture_content.hpp>
#include <transform.hpp>
#include <utility>
#include <variant>
#include <vector>
using namespace kuki;
namespace {
struct ShortcutDef {
  const char *label;
  const char *description;
  int key;
  int mods;
};
enum ShortcutIndex : int {
  ToggleFPS,
  Copy,
  Paste,
  ShortcutCount
};
constexpr ShortcutDef kShortcuts[ShortcutCount] = {
  {"Show FPS", "Toggle the FPS counter overlay in the viewport", GLFW_KEY_C, 0},
  {"Copy", "Copy the selected entities", GLFW_KEY_C, GLFW_MOD_CONTROL},
  {"Paste", "Paste the copied entities", GLFW_KEY_V, GLFW_MOD_CONTROL},
};
struct DebugViewSequenceDef {
  const char *sequence;
  const char *targetName;
  const char *description;
};
constexpr DebugViewSequenceDef kDebugViewSequences[] = {
  {"vf", "", "Show the final render output"},
  {"vs", "ShadowMap", "Show the directional light shadow map"},
};
auto ShortcutName(int index) -> std::string {
  return std::string("Editor.") + kShortcuts[index].label;
}
struct ModelAnimationSequence final : public ImSequencer::SequenceInterface {
  const AnimationClip *clip{};
  std::vector<std::array<int, 2>> ranges;
  auto GetFrameMin() const -> int override {
    return 0;
  }
  auto GetFrameMax() const -> int override {
    return clip ? static_cast<int>(clip->duration) : 0;
  }
  auto GetItemCount() const -> int override {
    return clip ? static_cast<int>(clip->channels.size()) : 0;
  }
  auto GetItemLabel(int index) const -> const char * override {
    if (!clip || index < 0 || index >= static_cast<int>(clip->channels.size()))
      return "";
    return clip->channels[index].nodeName.c_str();
  }
  void Get(int index, int **start, int **end, int *type, unsigned int *color) override {
    if (index >= 0 && index < static_cast<int>(ranges.size())) {
      if (start)
        *start = &ranges[index][0];
      if (end)
        *end = &ranges[index][1];
    }
    if (type)
      *type = 0;
    if (color)
      *color = IM_COL32(200, 160, 60, 100);
  }
  void CustomDraw(int index, ImDrawList *drawList, const ImRect &rc, const ImRect &, const ImRect &, const ImRect &) override {
    if (!clip || index < 0 || index >= static_cast<int>(clip->channels.size()))
      return;
    const auto &channel = clip->channels[index];
    const auto duration = clip->duration > 0.f ? clip->duration : 1.f;
    const auto y = (rc.Min.y + rc.Max.y) * 0.5f;
    const auto DrawKey = [&](const float time) {
      const auto x = rc.Min.x + (time / duration) * (rc.Max.x - rc.Min.x);
      constexpr auto half = 3.5f;
      const ImVec2 points[]{{x, y - half}, {x + half, y}, {x, y + half}, {x - half, y}};
      drawList->AddConvexPolyFilled(points, 4, IM_COL32(255, 200, 0, 255));
    };
    for (const auto &key : channel.positions)
      DrawKey(key.time);
    for (const auto &key : channel.rotations)
      DrawKey(key.time);
    for (const auto &key : channel.scales)
      DrawKey(key.time);
  }
};
} // namespace
Editor::Editor()
  : SystemApplication<ScriptingSystem, AnimationSystem, PhysicsSystem, RenderingSystem>({.name = "Kuki Editor", .iconPath = "image/kuki.ico"}) {}
auto Editor::Start() -> void {
  InitImGui();
  LoadDefaultScene();
  for (auto i = 0; i < ShortcutCount; ++i)
    RegisterBinding(ShortcutName(i), InputManager::Trigger{kShortcuts[i].key, kShortcuts[i].mods}, kShortcuts[i].description);
  RegisterInputAction(GLFW_MOUSE_BUTTON_RIGHT, [this]() { SetCursorLocked(true); auto &io = ImGui::GetIO(); io.ConfigFlags |= ImGuiConfigFlags_NoMouse; });
  RegisterInputAction(GLFW_MOUSE_BUTTON_RIGHT, [this]() { SetCursorLocked(false); auto &io = ImGui::GetIO(); io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse; }, false);
  for (const auto &def : kDebugViewSequences) {
    const std::string targetName = def.targetName;
    RegisterInputAction(def.sequence, [this, targetName]() { context.debugViewTarget = targetName; }, def.description);
  }
}
auto Editor::Update(const float deltaTime) -> void {
  UpdateIO();
  UpdateView();
}
auto Editor::Shutdown() -> void {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
auto Editor::SetInputCaptureActive(bool active) -> void {
  context.state = active ? EditorState::RebindingKey : EditorState::Normal;
}
auto Editor::GetSelectedEntity() const -> EntityID {
  return context.selectedEntityId;
}
auto Editor::GetSelectedEntities() const -> std::vector<EntityID> {
  return {context.selectedEntities.begin(), context.selectedEntities.end()};
}
auto Editor::InitImGui() -> void {
  auto constexpr FONT_SIZE = 16.f;
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  auto &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_DockingEnable;
  auto fontPath = std::filesystem::path{GetDescription().path / "font/Inter-VariableFont_opsz,wght.ttf"}.string();
  io.Fonts->AddFontFromFileTTF(fontPath.c_str(), FONT_SIZE);
  auto &style = ImGui::GetStyle();
  style.ChildRounding = .0f;
  style.FrameRounding = .0f;
  style.TabRounding = .0f;
  style.WindowRounding = .0f;
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();
  ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
  fileBrowser.SetTitle("Browse Files");
  sceneFileBrowser.SetTypeFilters({".json"});
}
auto Editor::InitLayout() -> void {
  // TODO: if an imgui.ini file exists, restore the layout from it
  static bool firstRun = true;
  if (!firstRun)
    return;
  firstRun = false;
  auto viewport = ImGui::GetMainViewport();
  const auto dockspaceId = ImGui::GetID("DockSpace");
  const auto viewportSize = viewport->Size;
  ImGui::DockBuilderRemoveNode(dockspaceId);
  ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspaceId, viewportSize);
  auto mainId = dockspaceId;
  ImGui::DockBuilderDockWindow("Scene", mainId);
  auto rightId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Right, .3f, nullptr, &mainId);
  ImGui::DockBuilderDockWindow("Hierarchy", rightId);
  ImGui::DockBuilderDockWindow("Settings", rightId);
  auto rightBottomId = ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Down, .5f, nullptr, &rightId);
  ImGui::DockBuilderDockWindow("Properties", rightBottomId);
  auto bottomId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, .3f, nullptr, &mainId);
  ImGui::DockBuilderDockWindow("Assets", bottomId);
  ImGui::DockBuilderDockWindow("Animation", bottomId);
  ImGui::DockBuilderFinish(dockspaceId);
}
auto Editor::LoadDefaultScene() -> void {
  const auto sceneName = "Main";
  CreateScene(sceneName);
  const auto scenePath = GetDescription().path / "scene/default.json";
  if (!SceneSerializer::Load(*this, scenePath)) {
    spdlog::error("[Editor] failed to load default scene: {}", scenePath.string());
    return;
  }
  AttachCameraController();
}
auto Editor::UpdateIO() -> void {
  static auto stateOld = EditorState::Normal;
  if (context.state != stateOld) {
    stateOld = context.state;
    if (context.state == EditorState::Rename || context.state == EditorState::RebindingKey)
      DisableKeys();
    else
      EnableKeys();
    if (context.state != EditorState::Normal) {
      context.keyState.reset();
      context.pressState.reset();
      context.releaseState.reset();
      return;
    }
  }
  const auto &io = ImGui::GetIO();
  context.keyState.set(static_cast<uint8_t>(KeyBit::Alt), io.KeyAlt);
  context.keyState.set(static_cast<uint8_t>(KeyBit::Ctrl), io.KeyCtrl);
  context.keyState.set(static_cast<uint8_t>(KeyBit::Shift), io.KeyShift);
  context.pressState.set(static_cast<uint8_t>(KeyBit::Backspace), ImGui::IsKeyPressed(ImGuiKey_Backspace));
  context.pressState.set(static_cast<uint8_t>(KeyBit::Delete), ImGui::IsKeyPressed(ImGuiKey_Delete));
  context.pressState.set(static_cast<uint8_t>(KeyBit::Enter), ImGui::IsKeyPressed(ImGuiKey_Enter));
  context.pressState.set(static_cast<uint8_t>(KeyBit::Escape), ImGui::IsKeyPressed(ImGuiKey_Escape));
  context.pressState.set(static_cast<uint8_t>(KeyBit::Space), ImGui::IsKeyPressed(ImGuiKey_Space));
  context.releaseState.set(static_cast<uint8_t>(KeyBit::Backspace), ImGui::IsKeyReleased(ImGuiKey_Backspace));
  context.releaseState.set(static_cast<uint8_t>(KeyBit::Delete), ImGui::IsKeyReleased(ImGuiKey_Delete));
  context.releaseState.set(static_cast<uint8_t>(KeyBit::Enter), ImGui::IsKeyReleased(ImGuiKey_Enter));
  context.releaseState.set(static_cast<uint8_t>(KeyBit::Escape), ImGui::IsKeyReleased(ImGuiKey_Escape));
  context.releaseState.set(static_cast<uint8_t>(KeyBit::Space), ImGui::IsKeyReleased(ImGuiKey_Space));
}
auto Editor::UpdateView() -> void {
  const auto wasPicking = context.state == EditorState::PickingAsset;
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGuizmo::BeginFrame();
  ImGui::DockSpaceOverViewport(ImGui::GetID("DockSpace"));
  InitLayout();
  DisplayAssets();
  DisplayAnimation();
  DisplayHierarchy();
  DisplayProperties();
  DisplaySettings();
  DisplayScene();
  if (wasPicking && context.state == EditorState::PickingAsset && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    context.state = EditorState::Normal;
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
auto Editor::ApplyPickedAsset(const AssetID assetId) -> void {
  switch (context.pickingTarget) {
  case PickingTarget::MaterialTexture: {
    auto *material = GetEntityComponent<GLMaterial>(context.pickingEntityId);
    if (material) {
      const auto texture = static_cast<GLTexture *>(PreviewAsset(assetId));
      if (texture) {
        switch (context.pickingProperty) {
        case MaterialProperty::AlbedoTexture:
          material->textures.albedo = texture->id;
          break;
        case MaterialProperty::NormalTexture:
          material->textures.normal = texture->id;
          break;
        case MaterialProperty::MetalnessTexture:
          material->textures.metalness = texture->id;
          break;
        case MaterialProperty::OcclusionTexture:
          material->textures.occlusion = texture->id;
          break;
        case MaterialProperty::RoughnessTexture:
          material->textures.roughness = texture->id;
          break;
        case MaterialProperty::SpecularTexture:
          material->textures.specular = texture->id;
          break;
        case MaterialProperty::EmissiveTexture:
          material->textures.emissive = texture->id;
          break;
        }
      }
    }
    break;
  }
  case PickingTarget::Skybox: {
    auto *handle = GetEntityComponent<SkyboxHandle>(context.pickingEntityId);
    if (!handle)
      handle = AddEntityComponent<SkyboxHandle>(context.pickingEntityId);
    if (!GetEntityComponent<GLSkybox>(context.pickingEntityId))
      AddEntityComponent<GLSkybox>(context.pickingEntityId);
    if (handle && handle->assetId != assetId) {
      handle->assetId = assetId;
      handle->resourceId = EntityID::Invalid;
    }
    break;
  }
  }
  context.state = EditorState::Normal;
  context.pickingEntityId = EntityID::Invalid;
}
auto Editor::DisplayAssets() -> void {
  const auto previewSize = static_cast<float>(GetPreviewSize());
  ImGui::Begin("Assets");
  DisplayAssetBrowser(previewSize);
  ImGui::End();
}
auto Editor::DisplayAssetBrowser(const float previewSize) -> void {
  static constexpr ImVec2 UV0(0.f, 1.f);
  static constexpr ImVec2 UV1(1.f, 0.f);
  static constexpr ImVec2 FLIPPED_UV0(0.f, 0.f);
  static constexpr ImVec2 FLIPPED_UV1(1.f, 1.f);
  static constexpr auto POPUP_WINDOW_FLAGS = ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight;
  static constexpr auto LIST_ITEM_WIDTH = 160.f;
  const auto escapePressed = context.pressState.test(static_cast<uint8_t>(KeyBit::Escape));
  const auto picking = context.state == EditorState::PickingAsset;
  if (picking) {
    const auto min = ImGui::GetWindowPos();
    const auto max = ImVec2(min.x + ImGui::GetWindowSize().x, min.y + ImGui::GetWindowSize().y);
    ImGui::GetWindowDrawList()->AddRect(min, max, PICKING_HIGHLIGHT_COLOR, 0.f, 0, PICKING_HIGHLIGHT_THICKNESS);
    if (escapePressed)
      context.state = EditorState::Normal;
  }
  const auto pickingSkybox = picking && context.pickingTarget == PickingTarget::Skybox;
  std::vector<const Asset *> assets;
  auto CollectAsset = [&](const Asset *asset) {
    if (pickingSkybox) {
      const auto textureAsset = asset->As<TextureAsset>();
      if (!textureAsset || textureAsset->texture.content != TextureContent::Skybox)
        return;
    }
    assets.push_back(asset);
  };
  if (picking)
    ForEachAsset(context.pickingAssetType, CollectAsset);
  else
    ForEachAssetType([this, &CollectAsset](const AssetType type, const std::string &) { ForEachAsset(type, CollectAsset); });
  const auto DisplayContextMenu = [this] {
    if (ImGui::BeginPopupContextWindow("AssetBrowserMenu", POPUP_WINDOW_FLAGS)) {
      if (ImGui::MenuItem("Import")) {
        fileBrowser.Open();
        ImGui::CloseCurrentPopup();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("List View", nullptr, context.assetViewMode == AssetViewMode::List))
        context.assetViewMode = AssetViewMode::List;
      if (ImGui::MenuItem("Thumbnail View", nullptr, context.assetViewMode == AssetViewMode::Thumbnail))
        context.assetViewMode = AssetViewMode::Thumbnail;
      ImGui::EndPopup();
    }
  };
  const auto &style = ImGui::GetStyle();
  if (context.assetViewMode == AssetViewMode::List) {
    const auto rowHeight = ImGui::GetTextLineHeightWithSpacing();
    const auto rowsPerColumn = (std::max)(1, static_cast<int>(ImGui::GetContentRegionAvail().y / rowHeight));
    const auto selectableWidth = LIST_ITEM_WIDTH - style.ItemSpacing.x;
    ImGui::BeginChild("##AssetList", ImVec2(0.f, 0.f), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (size_t i = 0; i < assets.size(); ++i) {
      const auto id = assets[i]->id;
      const auto column = static_cast<int>(i) / rowsPerColumn;
      const auto row = static_cast<int>(i) % rowsPerColumn;
      ImGui::SetCursorPos(ImVec2(column * LIST_ITEM_WIDTH, row * rowHeight));
      const auto name = GetAssetName(id);
      ImGui::PushID(static_cast<int>(id));
      const auto selected = ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_None, ImVec2(selectableWidth, 0.f));
      if (picking && selected)
        ApplyPickedAsset(id);
      if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("##SpawnPayload", &id, sizeof(AssetID));
        ImGui::EndDragDropSource();
      }
      if (ImGui::IsItemHovered()) {
        const auto texture = static_cast<GLTexture *>(PreviewAsset(id));
        if (texture && ImGui::BeginTooltip()) {
          const auto &uv0 = texture->flipY ? FLIPPED_UV0 : UV0;
          const auto &uv1 = texture->flipY ? FLIPPED_UV1 : UV1;
          ImGui::ImageButton("##ListPreview", static_cast<ImTextureID>(texture->id), ImVec2(previewSize, previewSize), uv0, uv1);
          ImGui::EndTooltip();
        }
      }
      ImGui::PopID();
    }
    DisplayContextMenu();
    ImGui::EndChild();
  } else {
    const auto cellWidth = previewSize + style.FramePadding.x * 2.f + style.ItemSpacing.x;
    const auto columnCount = static_cast<size_t>((std::max)(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cellWidth)));
    ImGui::BeginChild("##AssetThumbnails", ImVec2(0.f, 0.f), false);
    for (size_t i = 0; i < assets.size(); ++i) {
      const auto id = assets[i]->id;
      const auto name = GetAssetName(id);
      if (i % columnCount != 0)
        ImGui::SameLine();
      ImGui::PushID(static_cast<int>(id));
      ImGui::BeginGroup();
      const auto texture = static_cast<GLTexture *>(PreviewAsset(id));
      const auto tex = texture ? static_cast<ImTextureID>(texture->id) : ImTextureID{};
      const auto &uv0 = texture && texture->flipY ? FLIPPED_UV0 : UV0;
      const auto &uv1 = texture && texture->flipY ? FLIPPED_UV1 : UV1;
      const auto clicked = ImGui::ImageButton("##Thumbnail", tex, ImVec2(previewSize, previewSize), uv0, uv1);
      if (picking && clicked)
        ApplyPickedAsset(id);
      if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("##SpawnPayload", &id, sizeof(AssetID));
        ImGui::EndDragDropSource();
      }
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + previewSize);
      ImGui::TextWrapped("%s", name.c_str());
      ImGui::PopTextWrapPos();
      ImGui::EndGroup();
      ImGui::PopID();
    }
    DisplayContextMenu();
    ImGui::EndChild();
  }
  fileBrowser.Display();
  if (fileBrowser.HasSelected()) {
    const auto filepath = fileBrowser.GetSelected();
    if (!std::filesystem::exists(filepath))
      spdlog::error("File does not exist: {}", filepath.string());
    else {
      const auto ext = filepath.extension();
      if (ext == ".vert" || ext == ".frag" || ext == ".comp")
        LoadAssetAsync<ShaderAsset>(filepath);
      else if (ext == ".gltf" || ext == ".glb" || ext == ".fbx")
        LoadAssetAsync<ModelAsset>(filepath);
      else if (ext == ".hdr" || ext == ".exr")
        LoadAssetAsync<TextureAsset>(filepath);
    }
    fileBrowser.ClearSelected();
  }
}
auto Editor::DisplayAnimation() -> void {
  ImGui::Begin("Animation");
  Animator *animator{};
  Skeleton *skeleton{};
  if (!context.selectedEntities.empty()) {
    auto id = *context.selectedEntities.begin();
    for (auto i = 0; i < 64 && id && !animator; ++i) {
      animator = GetEntityComponent<Animator>(id);
      skeleton = GetEntityComponent<Skeleton>(id);
      if (!animator)
        id = GetEntityParent(id);
    }
  }
  if (!animator || !skeleton) {
    ImGui::TextDisabled("Select an entity with an Animator component to preview its animation.");
    ImGui::End();
    return;
  }
  auto modelAsset = GetAsset<ModelAsset>(animator->modelAssetId);
  if (!modelAsset || animator->clipIndex < 0 || animator->clipIndex >= static_cast<int>(modelAsset->animations.size())) {
    ImGui::TextDisabled("Selected entity's Animator has no valid clip assigned.");
    ImGui::End();
    return;
  }
  auto &clip = modelAsset->animations[animator->clipIndex];
  if (ImGui::Button(animator->playing ? "Pause" : "Play"))
    animator->playing = !animator->playing;
  ImGui::SameLine();
  ImGui::Checkbox("Loop", &animator->loop);
  ImGui::SameLine();
  const auto seconds = clip.ticksPerSecond > 0.f ? clip.duration / clip.ticksPerSecond : 0.f;
  ImGui::TextDisabled("%s (%.2fs)", clip.name.c_str(), seconds);
  ModelAnimationSequence sequence;
  sequence.clip = &clip;
  sequence.ranges.assign(clip.channels.size(), {0, static_cast<int>(clip.duration)});
  auto currentFrame = static_cast<int>(animator->time);
  static bool expanded = true;
  auto selectedEntry = -1;
  auto firstFrame = 0;
  if (ImSequencer::Sequencer(&sequence, &currentFrame, &expanded, &selectedEntry, &firstFrame, ImSequencer::SEQUENCER_CHANGE_FRAME))
    animator->time = static_cast<float>(currentFrame);
  ImGui::End();
}
auto Editor::DisplayEntity(const EntityID id) -> void {
  static constexpr auto TREE_NODE_FLAGS = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_NavLeftJumpsToParent;
  static constexpr auto INPUT_TEXT_FLAGS = ImGuiTreeNodeFlags(ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
  static constexpr auto NAME_LENGTH = 256;
  static char newName[NAME_LENGTH] = "";
  static auto renameBufferOwner = EntityID::Invalid;
  auto nodeFlags = ImGuiTreeNodeFlags(TREE_NODE_FLAGS);
  if (!EntityHasChildren(id))
    nodeFlags |= ImGuiTreeNodeFlags_Leaf;
  if (context.selectedEntities.contains(id))
    nodeFlags |= ImGuiTreeNodeFlags_Selected;
  if (context.state == EditorState::Rename && context.renamedEntityId == id) {
    if (renameBufferOwner != id) {
      const auto currentName = GetEntityName(id);
      strncpy(newName, currentName.c_str(), NAME_LENGTH - 1);
      newName[NAME_LENGTH - 1] = '\0';
      renameBufferOwner = id;
    }
    ImGui::AlignTextToFramePadding();
    ImGui::PushItemWidth(-1);
    ImGui::SetKeyboardFocusHere();
    auto renamed = ImGui::InputText("##Rename", newName, NAME_LENGTH, INPUT_TEXT_FLAGS);
    if (renamed) {
      std::string nameStr = newName;
      if (!nameStr.empty())
        RenameEntity(id, nameStr);
    }
    if (renamed || ImGui::IsItemDeactivated()) {
      context.state = EditorState::Normal;
      context.renamedEntityId = EntityID::Invalid;
      renameBufferOwner = EntityID::Invalid;
    }
    ImGui::PopItemWidth();
    return;
  }
  const auto entityName = GetEntityName(id);
  const auto entityNameCStr = entityName.c_str();
  const auto nodeOpen = ImGui::TreeNodeEx(static_cast<const void *>(id), nodeFlags, "%s", entityNameCStr);
  displayedEntitiesNext.push_back(id);
  const auto doubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
  const auto focused = ImGui::IsItemFocused();
  const auto hovered = ImGui::IsItemHovered();
  const auto ctrlHeld = context.keyState.test(static_cast<uint8_t>(KeyBit::Ctrl));
  const auto enterPressed = context.pressState.test(static_cast<uint8_t>(KeyBit::Enter));
  const auto shiftHeld = context.keyState.test(static_cast<uint8_t>(KeyBit::Shift));
  const auto spacePressed = context.pressState.test(static_cast<uint8_t>(KeyBit::Space));
  const auto navPressed = ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_RightArrow);
  const auto clicked = (shiftHeld || ctrlHeld) ? ImGui::IsItemClicked(ImGuiMouseButton_Left) : (ImGui::IsItemDeactivated() && hovered);
  if (context.state == EditorState::Normal) {
    if (clicked) {
      if (shiftHeld) {
        auto it1 = std::find(displayedEntities.begin(), displayedEntities.end(), context.selectedEntityId);
        auto it2 = std::find(displayedEntities.begin(), displayedEntities.end(), id);
        if (it1 != displayedEntities.end() && it2 != displayedEntities.end()) {
          if (it1 > it2)
            std::swap(it1, it2);
          if (!ctrlHeld)
            context.selectedEntities.clear();
          for (auto it = it1; it <= it2; ++it)
            context.selectedEntities.insert(*it);
        }
      } else if (ctrlHeld) {
        if (context.selectedEntities.contains(id)) {
          context.selectedEntities.erase(id);
          context.selectedEntityId = EntityID::Invalid;
        } else
          context.selectedEntities.insert(id);
      } else {
        context.selectedEntities.clear();
        context.selectedEntities.insert(id);
      }
      context.selectedEntityId = id;
    } else if (focused && navPressed && (shiftHeld || ctrlHeld)) {
      context.selectedEntities.insert(id);
      context.selectedEntityId = id;
    } else if (focused && (enterPressed || spacePressed)) {
      context.selectedEntities.clear();
      context.selectedEntityId = id;
    }
  }
  if (hovered && doubleClicked) {
    context.state = EditorState::Rename;
    context.renamedEntityId = id;
  }
  if (context.state == EditorState::Normal && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
    if (!context.selectedEntities.contains(id)) {
      context.selectedEntities.clear();
      context.selectedEntities.insert(id);
      context.selectedEntityId = id;
    }
    ImGui::OpenPopup("##EntityContextMenu");
    entityContextMenuTriggered = true;
  }
  if (ImGui::BeginPopup("##EntityContextMenu")) {
    if (ImGui::MenuItem("Copy"))
      CopySelectedEntities();
    if (ImGui::MenuItem("Paste", nullptr, false, !entityClipboardRoots.empty()))
      PasteEntities();
    ImGui::Separator();
    if (ImGui::MenuItem("Delete"))
      for (const auto &entityId : context.selectedEntities)
        DeleteEntity(entityId);
    ImGui::EndPopup();
  }
  if (nodeOpen) {
    ForEachChildEntity(id, [&](const EntityID childId) {
      ImGui::PushID(static_cast<int>(childId));
      DisplayEntity(childId);
      ImGui::PopID();
    });
    ImGui::TreePop();
  }
}
auto Editor::DisplayHierarchy() -> void {
  static constexpr auto POPUP_WINDOW_FLAGS = ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight;
  ImGui::Begin("Hierarchy");
  const auto clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  const auto windowHovered = ImGui::IsWindowHovered();
  const auto itemsHovered = ImGui::IsAnyItemHovered();
  const auto backspacePressed = context.pressState.test(static_cast<uint8_t>(KeyBit::Backspace));
  const auto deletePressed = context.pressState.test(static_cast<uint8_t>(KeyBit::Delete));
  const auto escapePressed = context.pressState.test(static_cast<uint8_t>(KeyBit::Escape));
  const auto clearSelection = (windowHovered && !itemsHovered) && (clicked || backspacePressed || deletePressed || escapePressed);
  if (context.state == EditorState::Normal) {
    if (deletePressed)
      for (const auto &entityId : context.selectedEntities)
        DeleteEntity(entityId);
    if (IsBindingPressed(ShortcutName(Copy)))
      CopySelectedEntities();
    if (IsBindingPressed(ShortcutName(Paste)))
      PasteEntities();
    if (clearSelection) {
      context.selectedEntities.clear();
      context.selectedEntityId = EntityID::Invalid;
    }
  }
  displayedEntitiesNext.clear();
  entityContextMenuTriggered = false;
  ForEachRootEntity([this](const EntityID id) {
    ImGui::PushID(static_cast<int>(id));
    DisplayEntity(id);
    ImGui::PopID();
  });
  displayedEntities = std::move(displayedEntitiesNext);
  if (!entityContextMenuTriggered && ImGui::BeginPopupContextWindow("CreateMenu", POPUP_WINDOW_FLAGS)) {
    if (ImGui::MenuItem("New")) {
      const auto id = CreateEntity("Entity");
      context.state = EditorState::Rename;
      context.renamedEntityId = id;
      spdlog::info("[Editor] created a new entity.");
      ImGui::CloseCurrentPopup();
    }
    if (ImGui::MenuItem("Paste", nullptr, false, !entityClipboardRoots.empty())) {
      PasteEntities();
      ImGui::CloseCurrentPopup();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Save Scene...")) {
      sceneFileAction = SceneFileAction::Save;
      sceneFileBrowser.SetTitle("Save Scene");
      sceneFileBrowser.Open();
      ImGui::CloseCurrentPopup();
    }
    if (ImGui::MenuItem("Load Scene...")) {
      sceneFileAction = SceneFileAction::Load;
      sceneFileBrowser.SetTitle("Load Scene");
      sceneFileBrowser.Open();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  sceneFileBrowser.Display();
  if (sceneFileBrowser.HasSelected()) {
    auto path = sceneFileBrowser.GetSelected();
    if (path.extension() != ".json")
      path += ".json";
    if (sceneFileAction == SceneFileAction::Save)
      SaveScene(path);
    else
      LoadScene(path);
    sceneFileBrowser.ClearSelected();
  }
  ImGui::End();
}
auto Editor::SaveScene(const std::filesystem::path &path) -> void {
  if (SceneSerializer::Save(*this, path))
    spdlog::info("[Editor] saved scene to: {}", path.string());
  else
    spdlog::error("[Editor] failed to save scene to: {}", path.string());
}
auto Editor::LoadScene(const std::filesystem::path &path) -> void {
  if (!SceneSerializer::Load(*this, path)) {
    spdlog::error("[Editor] failed to load scene from: {}", path.string());
    return;
  }
  context.selectedEntities.clear();
  context.selectedEntityId = EntityID::Invalid;
  context.state = EditorState::Normal;
  displayedEntities.clear();
  AttachCameraController();
  spdlog::info("[Editor] loaded scene from: {}", path.string());
}
auto Editor::AttachCameraController() -> EntityID {
  auto cameraId = EntityID::Invalid;
  ForEachEntity<Camera>([&cameraId](const EntityID id, Camera *) {
    if (!cameraId)
      cameraId = id;
  });
  if (!cameraId)
    return cameraId;
  context.cameraEntity = cameraId;
  SetActiveCamera(cameraId);
  if (!GetEntityComponent<CameraController>(cameraId))
    AddEntityComponent<CameraController>(cameraId);
  return cameraId;
}
auto Editor::CopySelectedEntities() -> void {
  auto scene = GetScene();
  if (!scene || context.selectedEntities.empty())
    return;
  entityClipboard.Clear();
  entityClipboardRoots.clear();
  for (const auto &id : context.selectedEntities) {
    auto ancestorSelected = false;
    for (auto parent = GetEntityParent(id); parent; parent = GetEntityParent(parent))
      if (context.selectedEntities.contains(parent)) {
        ancestorSelected = true;
        break;
      }
    if (ancestorSelected)
      continue;
    if (const auto clipboardId = scene->CopyEntityTo(id, entityClipboard); clipboardId)
      entityClipboardRoots.emplace_back(clipboardId, GetEntityParent(id));
  }
  spdlog::info("[Editor] copied {} entities.", entityClipboardRoots.size());
}
auto Editor::PasteEntities() -> void {
  auto scene = GetScene();
  if (!scene || entityClipboardRoots.empty())
    return;
  context.selectedEntities.clear();
  context.selectedEntityId = EntityID::Invalid;
  for (const auto &[clipboardId, originalParent] : entityClipboardRoots) {
    const auto newId = scene->CopyEntityFrom(entityClipboard, clipboardId);
    if (!newId)
      continue;
    if (originalParent && IsEntity(originalParent))
      AddChildEntity(originalParent, newId);
    context.selectedEntities.insert(newId);
    context.selectedEntityId = newId;
  }
  spdlog::info("[Editor] pasted {} entities.", context.selectedEntities.size());
}
static auto IsHandleComponent(const ComponentType type) -> bool {
  switch (type) {
  case ComponentType::MaterialHandle:
  case ComponentType::MeshHandle:
  case ComponentType::ModelMaterialHandle:
  case ComponentType::ModelMeshHandle:
  case ComponentType::SkyboxHandle:
  case ComponentType::TextureHandle:
    return true;
  default:
    return false;
  }
}
auto Editor::DisplayProperties() -> void {
  static constexpr auto POPUP_WINDOW_FLAGS = ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight;
  if (!context.selectedEntityId)
    return;
  ImGui::Begin("Properties");
  auto componentTypes = GetEntityComponentTypes(context.selectedEntityId);
  for (auto i = 0; i < componentTypes.size(); ++i) {
    const auto componentType = componentTypes[i];
    if (IsHandleComponent(componentType))
      continue;
    ImGui::PushID(static_cast<int>(i));
    if (componentType == ComponentType::Script) {
      auto scripts = GetEntityComponent<Script>(context.selectedEntityId);
      auto scriptRemoved = false;
      for (size_t s = 0; s < scripts.size(); ++s) {
        auto *script = scripts[s];
        if (!script)
          continue;
        ImGui::PushID(static_cast<int>(s));
        const auto scriptOpen = ImGui::CollapsingHeader(script->GetName().c_str());
        if (ImGui::BeginPopupContextItem()) {
          if (ImGui::MenuItem("Remove"))
            scriptRemoved = RemoveEntityScript(context.selectedEntityId, script->GetTypeIndex());
          ImGui::EndPopup();
        }
        if (!scriptRemoved && scriptOpen)
          DisplayProperties(ComponentVariant{script});
        ImGui::PopID();
        if (scriptRemoved)
          break;
      }
      ImGui::PopID();
      if (scriptRemoved) {
        ImGui::End();
        return;
      }
      continue;
    }
    const auto name = Component::GetTypeName(componentType);
    const auto open = ImGui::CollapsingHeader(name.c_str());
    auto removed = false;
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Remove")) {
        removed = RemoveComponentByType(*this, context.selectedEntityId, componentType);
      }
      ImGui::EndPopup();
    }
    if (!removed && open) {
      auto componentOpt = GetEntityComponent(context.selectedEntityId, componentType);
      if (componentOpt.has_value())
        DisplayProperties(componentOpt.value());
    }
    ImGui::PopID();
    if (removed) {
      ImGui::End();
      return;
    }
  }
  if (ImGui::BeginPopupContextWindow("AddComponent", POPUP_WINDOW_FLAGS)) {
    auto availableComponents = GetMissingEntityComponents(context.selectedEntityId);
    for (const auto &compType : availableComponents)
      if (!IsHandleComponent(compType) && !Component::IsGL(compType) && compType != ComponentType::Script && ImGui::MenuItem(Component::GetTypeName(compType).c_str()))
        AddComponentByType(*this, context.selectedEntityId, compType);
    if (ImGui::BeginMenu("GL")) {
      for (const auto &compType : availableComponents)
        if (Component::IsGL(compType) && ImGui::MenuItem(Component::GetTypeName(compType).c_str()))
          AddComponentByType(*this, context.selectedEntityId, compType);
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Script")) {
      auto currentScripts = GetEntityComponent<Script>(context.selectedEntityId);
      for (const auto &[type, info] : ScriptRegistry::GetTypes()) {
        const auto alreadyPresent = std::any_of(currentScripts.begin(), currentScripts.end(), [&](const Script *script) {
          return script && script->GetTypeIndex() == type;
        });
        if (!alreadyPresent && ImGui::MenuItem(info.name.c_str()))
          info.add(*this, context.selectedEntityId);
      }
      ImGui::EndMenu();
    }
    ImGui::EndPopup();
  }
  ImGui::End();
}
auto Editor::DisplayProperties(const ComponentVariant &variant) -> void {
  PropertyDisplayer displayer{context, *this};
  std::visit(displayer, variant);
}
auto Editor::DisplayScene() -> void {
  static constexpr auto SCENE_WINDOW_FLAGS = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  static constexpr ImVec2 UV0(0.f, 1.f);
  static constexpr ImVec2 UV1(1.f, 0.f);
  static constexpr float PICK_DRAG_THRESHOLD_SQ = 4.f * 4.f;
  ImGui::Begin("Scene", nullptr, SCENE_WINDOW_FLAGS);
  SetViewportHovered(ImGui::IsWindowHovered());
  if (IsBindingPressed(ShortcutName(ToggleFPS)))
    context.showFPS = !context.showFPS;
  auto renderingSystem = GetSystem<RenderingSystem>();
  if (renderingSystem) {
    const auto &contentRegion = ImGui::GetContentRegionAvail();
    const auto sceneWidth = static_cast<int>(contentRegion.x);
    const auto sceneHeight = static_cast<int>(contentRegion.y);
    SetResolution(sceneWidth, sceneHeight);
    const auto sceneTarget = static_cast<GLRenderTarget *>(renderingSystem->GetTarget(context.debugViewTarget));
    if (sceneTarget && sceneTarget->texture > 0) {
      const auto res = renderingSystem->GetResolution();
      ImGui::Image(sceneTarget->texture, ImVec2(res.first, res.second), UV0, UV1);
      const auto imageMin = ImGui::GetItemRectMin();
      const auto imageMax = ImGui::GetItemRectMax();
      if (ImGui::IsItemHovered() && GetButtonDown(GLFW_MOUSE_BUTTON_LEFT) && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
        pickPressPos = GetMousePosition();
        trackingClick = true;
      }
      if (trackingClick && GetButtonUp(GLFW_MOUSE_BUTTON_LEFT)) {
        trackingClick = false;
        const auto releasePos = GetMousePosition();
        const auto dx = releasePos.x - pickPressPos.x;
        const auto dy = releasePos.y - pickPressPos.y;
        const auto localX = releasePos.x - imageMin.x;
        const auto localY = releasePos.y - imageMin.y;
        const auto sizeX = imageMax.x - imageMin.x;
        const auto sizeY = imageMax.y - imageMin.y;
        if (dx * dx + dy * dy < PICK_DRAG_THRESHOLD_SQ && sizeX > 0.f && sizeY > 0.f && localX >= 0.f && localY >= 0.f && localX < sizeX && localY < sizeY) {
          const auto texelX = static_cast<int>(localX / sizeX * res.first);
          const auto texelY = static_cast<int>(localY / sizeY * res.second);
          const auto pickedId = renderingSystem->PickEntity(texelX, texelY);
          if (context.keyState.test(static_cast<uint8_t>(KeyBit::Shift))) {
            if (pickedId) {
              if (context.selectedEntities.contains(pickedId)) {
                context.selectedEntities.erase(pickedId);
                context.selectedEntityId = EntityID::Invalid;
              } else {
                context.selectedEntities.insert(pickedId);
                context.selectedEntityId = pickedId;
              }
            }
          } else {
            context.selectedEntities.clear();
            if (pickedId) {
              context.selectedEntities.insert(pickedId);
              context.selectedEntityId = pickedId;
            } else
              context.selectedEntityId = EntityID::Invalid;
          }
        }
      }
      DrawManipulator(res.first, res.second);
    }
  }
  if (ImGui::BeginDragDropTarget()) {
    if (auto payload = ImGui::AcceptDragDropPayload("##SpawnPayload")) {
      const auto assetIdPtr = static_cast<const AssetID *>(payload->Data);
      InstantiateAsset(*assetIdPtr);
      ImGui::SetWindowFocus();
    }
    ImGui::EndDragDropTarget();
  }
  if (context.showFPS) {
    ImGui::SetCursorPos(ImVec2(ImGui::GetTextLineHeight(), ImGui::GetFrameHeight() + ImGui::GetTextLineHeight()));
    ImGui::Text("%zu", GetFPS());
  }
  ImGui::End();
}
auto Editor::DisplaySettings() -> void {
  ImGui::Begin("Settings");
  ImGui::Checkbox("Show FPS", &context.showFPS);
  auto previewSize = GetPreviewSize();
  if (ImGui::SliderInt("Asset Preview Size", &previewSize, 32, 256, "%d", ImGuiSliderFlags_AlwaysClamp))
    SetPreviewSize(previewSize);
  if (auto renderingSystem = GetSystem<RenderingSystem>(); renderingSystem && ImGui::BeginCombo("Debug View", context.debugViewTarget.empty() ? "Final" : context.debugViewTarget.c_str())) {
    if (ImGui::Selectable("Final", context.debugViewTarget.empty()))
      context.debugViewTarget.clear();
    renderingSystem->ForEachTarget([this](const std::string &name, const TargetDescription &desc) {
      if (desc.type != TargetType::Texture2D)
        return;
      if (ImGui::Selectable(name.c_str(), context.debugViewTarget == name))
        context.debugViewTarget = name;
    });
    ImGui::EndCombo();
  }
  ImGui::Separator();
  ImGui::Text("Shortcuts");
  const auto &bindingNames = GetBindingNames();
  std::string currentCategory;
  for (auto i = 0; i < static_cast<int>(bindingNames.size()); ++i) {
    const auto &name = bindingNames[i];
    const auto dot = name.find('.');
    const auto category = dot == std::string::npos ? std::string() : name.substr(0, dot);
    const auto label = dot == std::string::npos ? name : name.substr(dot + 1);
    if (category != currentCategory) {
      currentCategory = category;
      ImGui::SeparatorText(currentCategory.c_str());
    }
    DisplayKeyBindingRow(*this, label.c_str(), GetBindingDescription(name), name, shortcutRebindingIndex, i);
  }
  ImGui::Separator();
  ImGui::Text("Sequences");
  for (const auto &sequence : GetSequenceNames())
    DisplaySequenceRow(sequence, GetSequenceDescription(sequence));
  ImGui::End();
}
auto Editor::DrawManipulator(const float width, const float height) -> void {
  if (context.selectedEntities.empty())
    return;
  auto cameraController = GetEntityComponent<CameraController>(context.cameraEntity);
  if (!cameraController) {
    AttachCameraController();
    cameraController = GetEntityComponent<CameraController>(context.cameraEntity);
    if (!cameraController)
      return;
  }
  const auto windowPos = ImGui::GetWindowPos();
  ImGuizmo::SetRect(windowPos.x, windowPos.y, width, height);
  ImGuizmo::SetOrthographic(cameraController->GetType() == CameraType::Orthographic);
  struct ManipulatorTarget {
    Transform *transformComp{};
    Camera *cameraComp{};
    Light *lightComp{};
    Transform current{};
  };
  std::vector<ManipulatorTarget> targets;
  targets.reserve(context.selectedEntities.size());
  for (const auto &id : context.selectedEntities) {
    ManipulatorTarget target;
    target.transformComp = GetEntityComponent<Transform>(id);
    if (target.transformComp)
      target.current = *target.transformComp;
    else {
      target.cameraComp = GetEntityComponent<Camera>(id);
      if (target.cameraComp)
        target.current = target.cameraComp->GetTransform();
      else {
        target.lightComp = GetEntityComponent<Light>(id);
        if (target.lightComp)
          target.current = target.lightComp->GetTransform();
        else
          continue;
      }
    }
    targets.push_back(target);
  }
  if (targets.empty())
    return;
  glm::mat4 pivot;
  if (targets.size() == 1)
    pivot = targets[0].current.world;
  else {
    glm::vec3 centroid{};
    for (const auto &target : targets)
      centroid += glm::vec3(target.current.world[3]);
    centroid /= static_cast<float>(targets.size());
    pivot = glm::translate(glm::mat4(1.f), centroid);
  }
  const auto pivotBefore = pivot;
  ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
  if (!ImGuizmo::Manipulate(glm::value_ptr(cameraController->GetView()), glm::value_ptr(cameraController->GetProjection()), ImGuizmo::OPERATION::UNIVERSAL, ImGuizmo::MODE::WORLD, glm::value_ptr(pivot)))
    return;
  const auto delta = pivot * glm::inverse(pivotBefore);
  for (auto &target : targets) {
    const auto newWorld = delta * target.current.world;
    glm::vec3 rotation;
    Transform result;
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(newWorld), glm::value_ptr(result.position), glm::value_ptr(rotation), glm::value_ptr(result.scale));
    result.rotation = glm::quat(glm::radians(rotation));
    result.world = newWorld;
    if (target.cameraComp)
      target.cameraComp->SetTransform(result);
    else if (target.lightComp)
      target.lightComp->SetTransform(result);
    else
      *target.transformComp = result;
  }
}
