#include <algorithm>
#include <application.hpp>
#include <application_description.hpp>
#include <bone_data.hpp>
#include <camera.hpp>
#include <camera_controller.hpp>
#include <component.hpp>
#include <editor.hpp>
#include <filesystem>
#include <gl_material.hpp>
#include <gl_mesh.hpp>
#include <gl_render_target.hpp>
#include <gl_shader.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <id.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <light.hpp>
#include <material_type.hpp>
#include <primitive.hpp>
#include <property_displayer.hpp>
#include <rendering_system.hpp>
#include <shader_asset.hpp>
#include <shader_type.hpp>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <string>
#include <transform.hpp>
#include <utility>
#include <vector>
//
#include <GLFW/glfw3.h>
#include <ImGuizmo.h>
#include <imfilebrowser.h>
using namespace kuki;
Editor::Editor()
  : Application({.name = "Kuki Editor", .iconPath = "image/kuki.ico"}) {}
auto Editor::Start() -> void {
  InitImGui();
  LoadDefaultAssets();
  LoadDefaultScene();
  RegisterInputAction(GLFW_MOUSE_BUTTON_RIGHT, [this]() {context.state = EditorState::Fly; glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); auto &io = ImGui::GetIO(); io.ConfigFlags |= ImGuiConfigFlags_NoMouse; });
  RegisterInputAction(GLFW_MOUSE_BUTTON_RIGHT, [this]() {context.state = EditorState::Normal; glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); auto &io = ImGui::GetIO(); io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse; }, false);
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
auto Editor::InitImGui() -> void {
  auto constexpr FONT_SIZE = 16.f;
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  auto &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_DockingEnable;
  auto fontPath = std::filesystem::path{GetDescription().path / "font/Inter-VariableFont_opsz,wght.ttf"}.string();
  io.Fonts->AddFontFromFileTTF(fontPath.c_str(), FONT_SIZE);
  auto &style = ImGui::GetStyle();
  // style.ChildBorderSize = .0f;
  style.ChildRounding = .0f;
  // style.FrameBorderSize = .0f;
  // style.FramePadding = ImVec2(.0f, .0f);
  style.FrameRounding = .0f;
  // style.ItemInnerSpacing = ImVec2(.0f, .0f);
  // style.ItemSpacing = ImVec2(.0f, .0f);
  // style.TabBorderSize = .0f;
  // style.TabCloseButtonMinWidthSelected = .0f;
  style.TabRounding = .0f;
  // style.WindowBorderSize = .0f;
  // style.WindowPadding = ImVec2(.0f, .0f);
  style.WindowRounding = .0f;
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();
  ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
  fileBrowser.SetTitle("Browse Files");
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
  const auto rightBottomId = ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Down, .5f, nullptr, &rightId);
  ImGui::DockBuilderDockWindow("Properties", rightBottomId);
  auto bottomId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, .3f, nullptr, &mainId);
  ImGui::DockBuilderDockWindow("Assets", bottomId);
  const auto bottomLeftId = ImGui::DockBuilderSplitNode(bottomId, ImGuiDir_Left, .2f, nullptr, &bottomId);
  ImGui::DockBuilderDockWindow("Categories", bottomLeftId);
  ImGui::DockBuilderFinish(dockspaceId);
}
auto Editor::LoadDefaultAssets() -> void {
  const auto &desc = GetDescription();
  // TODO: make LoadAsset<T> accept arbitrary arguments (e.g., shader type) to declutter this function
  // LoadAsset<ShaderAsset>(desc.path / "shader/standard_mvp.vert", "StandardMVP");
  auto vertId = LoadAsset<ShaderAsset>(desc.path / "shader/standard_m.vert", "Standard");
  GetAsset(vertId)->As<ShaderAsset>()->shaderType = ShaderType::Vertex;
  auto fragId = LoadAsset<ShaderAsset>(desc.path / "shader/bloom.frag", "Bloom");
  GetAsset(fragId)->As<ShaderAsset>()->vertexShader = vertId;
  fragId = LoadAsset<ShaderAsset>(desc.path / "shader/blur.frag", "Blur");
  GetAsset(fragId)->As<ShaderAsset>()->vertexShader = vertId;
  fragId = LoadAsset<ShaderAsset>(desc.path / "shader/bright_pass.frag", "BrightPass");
  GetAsset(fragId)->As<ShaderAsset>()->vertexShader = vertId;
  fragId = LoadAsset<ShaderAsset>(desc.path / "shader/gamma_correction.frag", "GammaCorrect");
  GetAsset(fragId)->As<ShaderAsset>()->vertexShader = vertId;
  vertId = LoadAsset<ShaderAsset>(desc.path / "shader/lit.vert", "Lit");
  auto shader = GetAsset(vertId)->As<ShaderAsset>();
  shader->shaderType = ShaderType::Vertex;
  shader->materialType = MaterialType::Lit;
  fragId = LoadAsset<ShaderAsset>(desc.path / "shader/lit.frag", "Lit");
  shader = GetAsset(fragId)->As<ShaderAsset>();
  shader->vertexShader = vertId;
  shader->materialType = MaterialType::Lit;
  //vertId = LoadAsset<ShaderAsset>(desc.path / "shader/lit_skinned.vert", "LitSkinned");
  //GetAsset(vertId)->As<ShaderAsset>()->shaderType = ShaderType::Vertex;
  //fragId = LoadAsset<ShaderAsset>(desc.path / "shader/lit.frag", "LitSkinned");
  //GetAsset(fragId)->As<ShaderAsset>()->vertexShader = vertId;
  vertId = LoadAsset<ShaderAsset>(desc.path / "shader/skybox.vert", "Skybox");
  GetAsset(vertId)->As<ShaderAsset>()->shaderType = ShaderType::Vertex;
  fragId = LoadAsset<ShaderAsset>(desc.path / "shader/skybox.frag", "Skybox");
  GetAsset(fragId)->As<ShaderAsset>()->vertexShader = vertId;
  vertId = LoadAsset<ShaderAsset>(desc.path / "shader/unlit.vert", "Unlit");
  GetAsset(vertId)->As<ShaderAsset>()->shaderType = ShaderType::Vertex;
  fragId = LoadAsset<ShaderAsset>(desc.path / "shader/unlit.frag", "Unlit");
  GetAsset(fragId)->As<ShaderAsset>()->vertexShader = vertId;
  auto compId = LoadAsset<ShaderAsset>(desc.path / "shader/brdf_lut.comp", "BRDF_LUT");
  GetAsset(compId)->As<ShaderAsset>()->shaderType = ShaderType::Compute;
  compId = LoadAsset<ShaderAsset>(desc.path / "shader/cubemap_equirect.comp", "CubemapEquirect");
  GetAsset(compId)->As<ShaderAsset>()->shaderType = ShaderType::Compute;
  compId = LoadAsset<ShaderAsset>(desc.path / "shader/equirect_cubemap.comp", "EquirectCubemap");
  GetAsset(compId)->As<ShaderAsset>()->shaderType = ShaderType::Compute;
  compId = LoadAsset<ShaderAsset>(desc.path / "shader/irradiance.comp", "IrradianceMap");
  GetAsset(compId)->As<ShaderAsset>()->shaderType = ShaderType::Compute;
  compId = LoadAsset<ShaderAsset>(desc.path / "shader/prefilter.comp", "PrefilterMap");
  GetAsset(compId)->As<ShaderAsset>()->shaderType = ShaderType::Compute;
}
auto Editor::LoadDefaultScene() -> void {
  const auto sceneName = "Main";
  CreateScene(sceneName);
  auto entityId = CreateEntity("Camera");
  context.cameraEntity = entityId;
  auto camera = AddEntityComponent<Camera>(entityId);
  camera->position = {3.2f, 1.4f, 2.2f};
  camera->rotation = glm::quat(glm::radians(glm::vec3(-24.f, 52.f, .0f)));
  ++camera->dirty;
  auto script = AddEntityComponent<CameraController>(entityId);
  script->entityId = entityId;
  entityId = CreateEntity("Skybox");
  AddEntityComponent<GLSkybox>(entityId);
  LoadScene(sceneName);
  InstantiateAsset("Cube");
}
auto Editor::UpdateIO() -> void {
  static auto stateOld = EditorState::Normal;
  if (context.state != stateOld) {
    stateOld = context.state;
    if (context.state == EditorState::Rename)
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
  context.pressState.set(static_cast<uint8_t>(KeyBit::F), ImGui::IsKeyPressed(ImGuiKey_F));
  context.pressState.set(static_cast<uint8_t>(KeyBit::Space), ImGui::IsKeyPressed(ImGuiKey_Space));
  context.releaseState.set(static_cast<uint8_t>(KeyBit::Backspace), ImGui::IsKeyReleased(ImGuiKey_Backspace));
  context.releaseState.set(static_cast<uint8_t>(KeyBit::Delete), ImGui::IsKeyReleased(ImGuiKey_Delete));
  context.releaseState.set(static_cast<uint8_t>(KeyBit::Enter), ImGui::IsKeyReleased(ImGuiKey_Enter));
  context.releaseState.set(static_cast<uint8_t>(KeyBit::Escape), ImGui::IsKeyReleased(ImGuiKey_Escape));
  context.releaseState.set(static_cast<uint8_t>(KeyBit::F), ImGui::IsKeyPressed(ImGuiKey_F));
  context.releaseState.set(static_cast<uint8_t>(KeyBit::Space), ImGui::IsKeyReleased(ImGuiKey_Space));
}
auto Editor::UpdateView() -> void {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGuizmo::BeginFrame();
  ImGui::DockSpaceOverViewport(ImGui::GetID("DockSpace"));
  InitLayout();
  DisplayAssetCategories();
  DisplayAssets();
  DisplayHierarchy();
  DisplayProperties();
  DisplayScene();
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
auto Editor::DisplayAssetCategories() -> void {
  ImGui::Begin("Categories");
  ForEachAssetType([this](const AssetType type, const std::string &name) {
    if (ImGui::Selectable(name.c_str(), context.selectedAssetType == type))
      context.selectedAssetType = type;
  });
  ImGui::End();
}
auto Editor::DisplayAssets() -> void {
  static constexpr auto POPUP_WINDOW_FLAGS = ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight;
  static constexpr auto PREVIEW_SIZE = 128;
  ImGui::Begin("Assets");
  const auto clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  const auto windowHovered = ImGui::IsWindowHovered();
  const auto itemsHovered = ImGui::IsAnyItemHovered();
  const auto backspacePressed = context.pressState.test(static_cast<uint8_t>(KeyBit::Backspace));
  const auto deletePressed = context.pressState.test(static_cast<uint8_t>(KeyBit::Delete));
  const auto escapePressed = context.pressState.test(static_cast<uint8_t>(KeyBit::Escape));
  const auto clearSelection = (windowHovered && !itemsHovered) && (clicked || backspacePressed || deletePressed || escapePressed);
  if (context.state == EditorState::Normal && clearSelection) {
    // context.selectedAssets.clear();
    context.selectedAssetID = AssetID::Invalid;
  }
  ForEachAsset(context.selectedAssetType, [this](const AssetID id, const std::string &name) {
    ImGui::PushID(static_cast<int>(id));
    const auto selected = context.selectedAssetID == id;
    if (ImGui::Selectable(name.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
      context.selectedAssetID = id;
    if (ImGui::IsItemHovered()) {
      const auto texture = static_cast<GLTexture *>(PreviewAsset(id));
      if (texture) {
        ImGui::BeginTooltip();
        ImGui::Image(texture->id, ImVec2(PREVIEW_SIZE, PREVIEW_SIZE));
        ImGui::EndTooltip();
      }
    }
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
      ImGui::SetDragDropPayload("##SpawnPayload", &id, sizeof(AssetID));
      ImGui::Text("%s", name.c_str());
      ImGui::EndDragDropSource();
    }
    ImGui::PopID();
  });
  if (ImGui::BeginPopupContextWindow("ImportAsset", POPUP_WINDOW_FLAGS)) {
    if (ImGui::MenuItem("Import")) {
      fileBrowser.Open();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
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
      else if (ext == ".gltf")
        LoadAssetAsync<SceneAsset>(filepath);
      else if (ext == ".hdr" || ext == ".exr")
        LoadAssetAsync<SkyboxAsset>(filepath);
    }
    fileBrowser.ClearSelected();
  }
  ImGui::End();
}
auto Editor::DisplayEntity(const EntityID id) -> void {
  static constexpr auto TREE_NODE_FLAGS = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_NavLeftJumpsToParent;
  static constexpr auto INPUT_TEXT_FLAGS = ImGuiTreeNodeFlags(ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
  static constexpr auto NAME_LENGTH = 256;
  static char newName[NAME_LENGTH] = "";
  auto nodeFlags = ImGuiTreeNodeFlags(TREE_NODE_FLAGS);
  if (!EntityHasChildren(id))
    nodeFlags |= ImGuiTreeNodeFlags_Leaf;
  if (context.selectedEntities.contains(id))
    nodeFlags |= ImGuiTreeNodeFlags_Selected;
  if (context.state == EditorState::Rename && context.renamedEntityID == id) {
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
      context.renamedEntityID = EntityID::Invalid;
    }
    ImGui::PopItemWidth();
    return;
  }
  const auto entityName = GetEntityName(id);
  const auto entityNameCStr = entityName.c_str();
  const auto nodeOpen = ImGui::TreeNodeEx(static_cast<const void *>(id), nodeFlags, "%s", entityNameCStr);
  displayedEntities.push_back(id);
  const auto clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
  const auto doubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
  const auto focused = ImGui::IsItemFocused();
  const auto hovered = ImGui::IsItemHovered();
  const auto ctrlHeld = context.keyState.test(static_cast<uint8_t>(KeyBit::Ctrl));
  const auto enterPressed = context.pressState.test(static_cast<uint8_t>(KeyBit::Enter));
  const auto shiftHeld = context.keyState.test(static_cast<uint8_t>(KeyBit::Shift));
  const auto spacePressed = context.pressState.test(static_cast<uint8_t>(KeyBit::Space));
  if (context.state == EditorState::Normal) {
    if (clicked) {
      if (shiftHeld) {
        auto it1 = std::find(displayedEntities.begin(), displayedEntities.end(), context.selectedEntityID);
        auto it2 = std::find(displayedEntities.begin(), displayedEntities.end(), id);
        if (it1 != displayedEntities.end() && it2 != displayedEntities.end()) {
          if (it1 > it2)
            // if it1 is below it2 in the list, swap them
            std::swap(it1, it2);
          if (!ctrlHeld)
            // if CTRL is pressed, append range to selection; otherwise, replace selection with range
            context.selectedEntities.clear();
          for (auto it = it1; it <= it2; ++it)
            // add items between the two entities (inclusive) to selection
            context.selectedEntities.insert(*it);
        }
      } else if (ctrlHeld) {
        if (context.selectedEntities.contains(id)) {
          context.selectedEntities.erase(id);
          context.selectedEntityID = EntityID::Invalid;
        } else
          context.selectedEntities.insert(id);
      } else {
        context.selectedEntities.clear();
        context.selectedEntities.insert(id);
      }
      context.selectedEntityID = id;
    } else if (focused && (shiftHeld || ctrlHeld)) {
      context.selectedEntities.insert(id);
      context.selectedEntityID = id;
    } else if (focused && (enterPressed || spacePressed)) {
      context.selectedEntities.clear();
      context.selectedEntityID = id;
    }
  }
  if (hovered && doubleClicked) {
    strncpy(newName, entityNameCStr, NAME_LENGTH);
    context.state = EditorState::Rename;
    context.renamedEntityID = id;
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
    if (clearSelection) {
      context.selectedEntities.clear();
      context.selectedEntityID = EntityID::Invalid;
    }
    if (deletePressed)
      for (const auto &entityId : context.selectedEntities)
        DeleteEntity(entityId);
  }
  displayedEntities.clear();
  ForEachRootEntity([this](const EntityID id) {
    ImGui::PushID(static_cast<int>(id));
    DisplayEntity(id);
    ImGui::PopID();
  });
  if (ImGui::BeginPopupContextWindow("CreateMenu", POPUP_WINDOW_FLAGS)) {
    if (ImGui::MenuItem("New")) {
      CreateEntity("Entity");
      spdlog::info("Created a new entity.");
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  ImGui::End();
}
auto Editor::DisplayProperties() -> void {
  static constexpr auto POPUP_WINDOW_FLAGS = ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight;
  if (!context.selectedEntityID)
    return;
  ImGui::Begin("Properties");
  auto componentTypes = GetEntityComponentTypes(context.selectedEntityID);
  for (auto i = 0; i < componentTypes.size(); ++i) {
    const auto componentType = componentTypes[i];
    const auto isSelected = context.selectedComponentType == componentType;
    ImGui::PushID(static_cast<int>(i));
    const auto name = Component::GetTypeName(componentType);
    if (ImGui::Selectable(name.c_str(), isSelected))
      context.selectedComponentType = componentType;
    auto removed = false;
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Remove")) {
        RemoveEntityComponent(context.selectedEntityID, componentType);
        removed = true;
      }
      ImGui::EndPopup();
    }
    if (!removed) {
      auto componentOpt = GetEntityComponent(context.selectedEntityID, componentType);
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
    auto availableComponents = GetMissingEntityComponents(context.selectedEntityID);
    for (const auto &compType : availableComponents)
      if (ImGui::MenuItem(Component::GetTypeName(compType).c_str())) {
        AddEntityComponent(context.selectedEntityID, compType);
      }
    ImGui::EndPopup();
  }
  ImGui::End();
}
auto Editor::DisplayProperties(const ComponentVariant &variant) -> void {
  PropertyDisplayer displayer(*this);
  std::visit(displayer, variant);
}
auto Editor::DisplayScene() -> void {
  static constexpr auto SCENE_WINDOW_FLAGS = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  static constexpr ImVec2 UV0(0.f, 1.f);
  static constexpr ImVec2 UV1(1.f, 0.f);
  ImGui::Begin("Scene", nullptr, SCENE_WINDOW_FLAGS);
  const auto fPressed = context.pressState.test(static_cast<uint8_t>(KeyBit::F));
  if (context.state == EditorState::Normal && fPressed)
    context.showFPS = !context.showFPS;
  auto renderingSystem = GetSystem<RenderingSystem>();
  if (renderingSystem) {
    const auto sceneTarget = static_cast<GLRenderTarget *>(renderingSystem->GetTarget());
    if (sceneTarget && sceneTarget->texture > 0) {
      const auto &settings = GetSettings();
      ImGui::Image(sceneTarget->texture, ImVec2(settings.res.width, settings.res.height), UV0, UV1);
      DrawManipulator(settings.res.width, settings.res.height);
      const auto &contentRegion = ImGui::GetContentRegionAvail();
      const auto sceneWidth = static_cast<int>(contentRegion.x);
      const auto sceneHeight = static_cast<int>(contentRegion.y);
      // FIXME: this is called many times while resizing the window
      SetResolution(sceneWidth, sceneHeight);
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
auto Editor::DrawManipulator(const float width, const float height) -> void {
  if (!context.selectedEntityID)
    return;
  auto cameraController = GetEntityComponent<CameraController>(context.cameraEntity);
  if (!cameraController)
    return;
  const auto &settings = GetSettings();
  const auto windowPos = ImGui::GetWindowPos();
  ImGuizmo::SetRect(windowPos.x, windowPos.y, width, height);
  ImGuizmo::SetOrthographic(cameraController->GetType() == CameraType::Orthographic);
  // TODO: for a multi-select, position the gizmo at the center of the selection, and apply manipulations to all entities
  Transform transform;
  auto transformComp = GetEntityComponent<Transform>(context.selectedEntityID);
  Camera *cameraComp = nullptr;
  Light *lightComp = nullptr;
  if (!transformComp) {
    cameraComp = GetEntityComponent<Camera>(context.selectedEntityID);
    if (cameraComp)
      transform = cameraComp->GetTransform();
    else {
      lightComp = GetEntityComponent<Light>(context.selectedEntityID);
      if (lightComp)
        transform = lightComp->GetTransform();
      else
        return;
    }
  } else
    transform = *transformComp;
  ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
  if (!ImGuizmo::Manipulate(glm::value_ptr(cameraController->GetView()), glm::value_ptr(cameraController->GetProjection()), ImGuizmo::OPERATION::UNIVERSAL, ImGuizmo::MODE::WORLD, glm::value_ptr(transform.local)))
    return;
  glm::vec3 rotation;
  ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform.local), glm::value_ptr(transform.position), glm::value_ptr(rotation), glm::value_ptr(transform.scale));
  transform.rotation = glm::quat(glm::radians(rotation));
  if (cameraComp)
    cameraComp->SetTransform(transform);
  else if (lightComp)
    lightComp->SetTransform(transform);
  else
    *transformComp = transform;
}
