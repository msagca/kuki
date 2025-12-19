#include <algorithm>
#include <app_config.hpp>
#include <application.hpp>
#include <bone_data.hpp>
#include <camera.hpp>
#include <camera_controller.hpp>
#include <cmath>
#include <component.hpp>
#include <component_traits.hpp>
#include <cstdint>
#include <display_traits.hpp>
#include <editor.hpp>
#include <filesystem>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <id.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <imgui_sink.hpp>
#include <light.hpp>
#include <material.hpp>
#include <memory>
#include <mesh.hpp>
#include <mesh_filter.hpp>
#include <mesh_renderer.hpp>
#include <mutex>
#include <primitive.hpp>
#include <random>
#include <rendering_system.hpp>
#include <skybox.hpp>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <string.h>
#include <string>
#include <texture.hpp>
#include <transform.hpp>
#include <utility>
#include <variant>
#include <vector>
//
#include <GLFW/glfw3.h>
#include <ImGuizmo.h>
#include <imfilebrowser.h>
using namespace kuki;
Editor::Editor()
  : Application(AppConfig{"Kuki Editor", "image/logo.png"}), imguiSink(std::make_shared<ImGuiSink<std::mutex>>()), logger(std::make_shared<spdlog::logger>("Logger", imguiSink)) {}
Editor::~Editor() {
  Application::Shutdown();
}
void Editor::Init() {
  Application::Init();
  RegisterInputAction("gc", [this]() { ToggleGizmo(GizmoType::FrustumCulling); });
  RegisterInputAction("gf", [this]() { ToggleGizmo(GizmoType::ViewFrustum); });
  RegisterInputAction("gm", [this]() { ToggleGizmo(GizmoType::Manipulator); });
  RegisterInputAction(GLFW_KEY_V, RenderingSystem::ToggleWireframeMode);
  RegisterInputAction(GLFW_MOUSE_BUTTON_RIGHT, [this]() {
    cameraController->mouselook = true;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouse; });
  RegisterInputAction(GLFW_MOUSE_BUTTON_RIGHT, [this]() {
    cameraController->mouselook = false;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    auto& io = ImGui::GetIO();
    io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse; }, false);
  InitImGui();
  CreateSystem<RenderingSystem>(*this);
  RegisterCommand(new SpawnCommand(*this));
  RegisterCommand(new DeleteCommand(*this));
  // spdlog::register_logger(logger);
}
void Editor::Start() {
  Application::Start();
  LoadDefaultAssets();
  LoadDefaultScene();
}
void Editor::Update() {
  Application::Update();
  cameraController->Update(deltaTime);
}
void Editor::LateUpdate() {
  Application::LateUpdate();
  UpdateView();
}
void Editor::Shutdown() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  Application::Shutdown();
}
void Editor::UpdateView() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGuizmo::BeginFrame();
  ImGui::DockSpaceOverViewport(ImGui::GetID("DockSpace"));
  InitLayout();
  DisplayHierarchy();
  DisplayAssets();
  DisplayScene();
  // DisplayLogs();
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
void Editor::InitLayout() {
  // TODO: if an imgui.ini file exists, restore the layout from it
  static bool firstRun = true;
  if (!firstRun)
    return;
  firstRun = false;
  auto viewport = ImGui::GetMainViewport();
  auto dockspaceId = ImGui::GetID("DockSpace");
  auto viewportSize = viewport->Size;
  ImGui::DockBuilderRemoveNode(dockspaceId);
  ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspaceId, viewportSize);
  auto mainId = dockspaceId;
  ImGui::DockBuilderDockWindow("Scene", mainId);
  auto rightId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Right, .3f, nullptr, &mainId);
  ImGui::DockBuilderDockWindow("Hierarchy", rightId);
  auto rightBottomId = ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Down, .5f, nullptr, &rightId);
  ImGui::DockBuilderDockWindow("Properties", rightBottomId);
  auto bottomId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, .3f, nullptr, &mainId);
  ImGui::DockBuilderDockWindow("Assets", bottomId);
  // ImGui::DockBuilderDockWindow("Logs", bottomId);
  ImGui::DockBuilderFinish(dockspaceId);
}
void Editor::LoadDefaultScene() {
  auto scene = CreateScene("Main");
  std::string entityName = "Camera";
  auto entityId = CreateEntity(entityName);
  auto camera = AddEntityComponent<Camera>(entityId);
  cameraController = std::make_unique<CameraController>(*this, entityId);
  entityName = "Cube";
  entityId = CreateEntity(entityName);
  auto filter = AddEntityComponent<MeshFilter>(entityId);
  filter->mesh = *GetAssetComponent<Mesh>(GetAssetId("Cube"));
  AddEntityComponent<MeshRenderer>(entityId);
  AddEntityComponent<Transform>(entityId);
  camera->Frame(filter->mesh.bounds, 2.f);
  entityName = "Skybox";
  entityId = CreateEntity(entityName);
  filter = AddEntityComponent<MeshFilter>(entityId);
  filter->mesh = *GetAssetComponent<Mesh>(GetAssetId("CubeInverted"));
  AddEntityComponent<Skybox>(entityId);
}
void Editor::LoadDefaultAssets() {
  LoadPrimitive(PrimitiveType::Cube);
  LoadPrimitive(PrimitiveType::CubeInverted);
  LoadPrimitive(PrimitiveType::Cylinder);
  LoadPrimitive(PrimitiveType::Frame);
  LoadPrimitive(PrimitiveType::Plane);
  LoadPrimitive(PrimitiveType::Sphere);
}
void Editor::InitImGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_DockingEnable;
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();
  ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
  fileBrowser.SetTitle("Browse Files");
}
void Editor::DisplayScene() {
  static constexpr ImVec2 uv0(0.f, 1.f);
  static constexpr ImVec2 uv1(1.f, 0.f);
  ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  const auto fPressed = ImGui::IsKeyPressed(ImGuiKey_F);
  const auto focused = ImGui::IsWindowFocused();
  static auto wasFocused = false;
  if (focused != wasFocused) {
    if (!wasFocused)
      EnableKeys();
    else
      DisableKeys();
    wasFocused = focused;
  }
  if (focused && fPressed && !showConsole)
    showFPS = !showFPS;
  auto renderSystem = GetSystem<RenderingSystem>();
  if (renderSystem) {
    renderSystem->SetGizmoMask(context.gizmoMask);
    auto texture = renderSystem->RenderSceneToTexture(&cameraController->camera);
    if (texture > 0) {
      auto& config = GetConfig();
      auto width = config.screenWidth;
      auto height = config.screenHeight;
      auto contentRegion = ImGui::GetContentRegionAvail();
      auto scaleFactor = std::max(contentRegion.x / width, contentRegion.y / height);
      auto drawWidth = width * scaleFactor;
      auto drawHeight = height * scaleFactor;
      ImGui::Image(texture, ImVec2(drawWidth, drawHeight), uv0, uv1);
      auto manipulatorEnabled = (context.gizmoMask & static_cast<size_t>(GizmoMask::Manipulator)) != 0;
      if (manipulatorEnabled)
        DrawManipulator(drawWidth, drawHeight);
    }
  }
  if (ImGui::BeginDragDropTarget()) {
    if (auto payload = ImGui::AcceptDragDropPayload("SPAWN_ASSET")) {
      std::string droppedAssetName = (const char*)payload->Data;
      Instantiate(droppedAssetName);
      ImGui::SetWindowFocus();
    }
    ImGui::EndDragDropTarget();
  }
  if (showFPS) {
    ImGui::SetCursorPos(ImVec2(ImGui::GetTextLineHeight(), ImGui::GetFrameHeight() + ImGui::GetTextLineHeight()));
    ImGui::Text("%zu", GetFPS());
  }
  static char commandBuffer[256] = "";
  static bool displayMessage = false;
  static std::string commandMessage;
  if (showConsole && escapePressed) {
    showConsole = false;
    commandBuffer[0] = '\0';
    commandMessage.clear();
    displayMessage = false;
  } else if (focused && !showConsole && enterPressed)
    showConsole = true;
  if (showConsole) {
    ImVec2 consoleSize(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeightWithSpacing() * (displayMessage ? 3 : 1));
    ImGui::BeginChild("ConsoleChild", consoleSize, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
    if (displayMessage)
      ImGui::TextColored(ImVec4(1.f, .8f, 0.f, 1.f), "%s", commandMessage.c_str());
    ImGui::SetKeyboardFocusHere();
    if (ImGui::InputText("##Command", commandBuffer, IM_ARRAYSIZE(commandBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
      auto result = DispatchCommand(commandBuffer, commandMessage);
      commandBuffer[0] = '\0';
      displayMessage = result != 0;
      showConsole = displayMessage;
    }
    ImGui::EndChild();
  }
  ImGui::End();
}
std::string Editor::GetComponentName(IComponent* component) {
  if (!component)
    return "";
  if (auto camera = component->As<Camera>())
    return DisplayTraits<Camera>::GetName();
  if (auto light = component->As<Light>())
    return DisplayTraits<Light>::GetName();
  if (auto material = component->As<Material>())
    return DisplayTraits<Material>::GetName();
  if (auto mesh = component->As<Mesh>())
    return DisplayTraits<Mesh>::GetName();
  if (auto filter = component->As<MeshFilter>())
    return DisplayTraits<MeshFilter>::GetName();
  if (auto renderer = component->As<MeshRenderer>())
    return DisplayTraits<MeshRenderer>::GetName();
  if (auto skybox = component->As<Skybox>())
    return DisplayTraits<Skybox>::GetName();
  if (auto texture = component->As<Texture>())
    return DisplayTraits<Texture>::GetName();
  if (auto transform = component->As<Transform>())
    return DisplayTraits<Transform>::GetName();
  return "";
}
ComponentType Editor::GetComponentType(IComponent* component) {
  if (!component)
    return ComponentType::Unknown;
  if (auto camera = component->As<Camera>())
    return ComponentTraits<Camera>::GetType();
  if (auto light = component->As<Light>())
    return ComponentTraits<Light>::GetType();
  if (auto material = component->As<Material>())
    return ComponentTraits<Material>::GetType();
  if (auto mesh = component->As<Mesh>())
    return ComponentTraits<Mesh>::GetType();
  if (auto filter = component->As<MeshFilter>())
    return ComponentTraits<MeshFilter>::GetType();
  if (auto renderer = component->As<MeshRenderer>())
    return ComponentTraits<MeshRenderer>::GetType();
  if (auto skybox = component->As<Skybox>())
    return ComponentTraits<Skybox>::GetType();
  if (auto texture = component->As<Texture>())
    return ComponentTraits<Texture>::GetType();
  if (auto transform = component->As<Transform>())
    return ComponentTraits<Transform>::GetType();
  return ComponentType::Unknown;
}
void Editor::DisplayComponents() {
  if (!context.selectedEntity.IsValid())
    return;
  ImGui::Begin("Properties");
  const auto clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  const auto windowHovered = ImGui::IsWindowHovered();
  const auto itemsHovered = ImGui::IsAnyItemHovered();
  if (clicked && windowHovered && !itemsHovered)
    context.assetMask = -1;
  auto components = GetAllEntityComponents(context.selectedEntity);
  for (auto i = 0; i < components.size(); ++i) {
    const auto& component = components[i];
    const auto componentType = static_cast<int>(GetComponentType(component));
    const auto isSelected = context.selectedComponent == componentType;
    ImGui::PushID(static_cast<int>(i));
    const auto name = GetComponentName(component);
    if (ImGui::Selectable(name.c_str(), isSelected))
      context.selectedComponent = componentType;
    auto removed = false;
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Remove")) {
        RemoveEntityComponent(context.selectedEntity, static_cast<ComponentType>(componentType));
        if (context.selectedComponent == componentType)
          context.selectedComponent = -1;
        removed = true;
      }
      ImGui::EndPopup();
    }
    if (!removed)
      DisplayProperties(component);
    ImGui::PopID();
    if (removed) {
      ImGui::End();
      return;
    }
  }
  if (ImGui::BeginPopupContextWindow("Add", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
    auto availableComponents = GetMissingEntityComponents(context.selectedEntity);
    for (const auto& comp : availableComponents)
      if (ImGui::MenuItem(comp.c_str()))
        AddEntityComponent(context.selectedEntity, comp);
    ImGui::EndPopup();
  }
  ImGui::End();
}
enum class FileType : uint8_t {
  None,
  Model,
  Image
};
void Editor::DisplayAssets() {
  static constexpr ImVec2 uv0(0.f, 1.f);
  static constexpr ImVec2 uv1(1.f, 0.f);
  static constexpr auto THUMBNAIL_SIZE = 128.f;
  static constexpr ImVec2 TILE_SIZE(THUMBNAIL_SIZE, THUMBNAIL_SIZE);
  static constexpr auto TILE_PADDING = 2.f;
  static constexpr auto TEXT_PADDING = 2.f;
  static const auto TEXT_HEIGHT = ImGui::GetTextLineHeight();
  static constexpr auto TILE_TOTAL_WIDTH = THUMBNAIL_SIZE + TILE_PADDING;
  static const auto TILE_TOTAL_HEIGHT = THUMBNAIL_SIZE + TEXT_PADDING + TEXT_HEIGHT + TILE_PADDING;
  static auto fileType = FileType::None;
  auto renderSystem = GetSystem<RenderingSystem>();
  if (!renderSystem)
    return;
  ImGui::Begin("Assets");
  const auto clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  const auto windowHovered = ImGui::IsWindowHovered();
  const auto itemsHovered = ImGui::IsAnyItemHovered();
  if (clicked && windowHovered && !itemsHovered)
    context.assetMask = -1;
  const auto contentRegion = ImGui::GetContentRegionAvail();
  const auto tilesPerRow = std::max(1, static_cast<int>(contentRegion.x / TILE_TOTAL_WIDTH));
  const auto scrollY = ImGui::GetScrollY();
  const auto visibleHeight = ImGui::GetWindowHeight();
  auto cursorStartPos = ImGui::GetCursorPos();
  if (ImGui::BeginPopupContextWindow("ImportMenu", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
    if (ImGui::BeginMenu("Import")) {
      if (ImGui::Selectable("Image")) {
        fileType = FileType::Image;
        fileBrowser.SetTypeFilters({".exr", ".hdr"});
        fileBrowser.Open();
      }
      if (ImGui::Selectable("Model")) {
        fileType = FileType::Model;
        fileBrowser.SetTypeFilters({".gltf"});
        fileBrowser.Open();
      }
      ImGui::EndMenu();
    }
    ImGui::EndPopup();
  }
  std::vector<ID> assetIds;
  if (context.assetMask == -1)
    ForEachRootAsset([&assetIds](ID assetId) { assetIds.push_back(assetId); });
  else if ((context.assetMask & (static_cast<int>(ComponentMask::Texture) | static_cast<int>(ComponentMask::Skybox))) != 0) {
    ForEachAsset<Texture>([&assetIds](ID assetId, Texture* _) { assetIds.push_back(assetId); });
    ForEachAsset<Skybox>([&assetIds](ID assetId, Skybox* _) { assetIds.push_back(assetId); });
  }
  const auto totalRows = (assetIds.size() + tilesPerRow - 1) / tilesPerRow;
  const auto totalContentHeight = totalRows * TILE_TOTAL_HEIGHT;
  ImGui::Dummy(ImVec2(0, totalContentHeight));
  ImGui::SetCursorPos(cursorStartPos);
  for (auto i = 0; i < assetIds.size(); ++i) {
    const auto& id = assetIds[i];
    const auto row = i / tilesPerRow;
    const auto col = i % tilesPerRow;
    ImVec2 tilePos(cursorStartPos.x + col * TILE_TOTAL_WIDTH, cursorStartPos.y + row * TILE_TOTAL_HEIGHT);
    auto tileTop = tilePos.y;
    auto tileBottom = tilePos.y + TILE_TOTAL_HEIGHT;
    if (tileBottom >= scrollY && tileTop <= scrollY + visibleHeight) {
      ImGui::SetCursorPos(tilePos);
      ImGui::PushID(i);
      auto textureId = renderSystem->RenderAssetToTexture(id, THUMBNAIL_SIZE);
      auto assetName = GetAssetName(id);
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
      if (ImGui::ImageButton(std::to_string(textureId).c_str(), textureId, TILE_SIZE, uv0, uv1)) {
        // HACK: the following needs to be handled by a dedicated method
        if (context.selectedEntity.IsValid() && context.selectedComponent >= 0) {
          auto component = GetEntityComponent(context.selectedEntity, static_cast<ComponentType>(context.selectedComponent));
          if (component) {
            auto [assetTexture, assetSkybox] = GetAssetComponents<Texture, Skybox>(id);
            if (auto entityMaterial = component->As<Material>()) {
              if (auto entityLitMaterial = std::get_if<LitMaterial>(&entityMaterial->current)) {
                if (assetTexture)
                  switch (context.selectedProperty) {
                  case static_cast<int>(MaterialProperty::AlbedoTexture):
                    entityLitMaterial->data.albedo = assetTexture->id;
                    break;
                  case static_cast<int>(MaterialProperty::NormalTexture):
                    entityLitMaterial->data.normal = assetTexture->id;
                    break;
                  case static_cast<int>(MaterialProperty::MetalnessTexture):
                    entityLitMaterial->data.metalness = assetTexture->id;
                    break;
                  case static_cast<int>(MaterialProperty::OcclusionTexture):
                    entityLitMaterial->data.occlusion = assetTexture->id;
                    break;
                  case static_cast<int>(MaterialProperty::RoughnessTexture):
                    entityLitMaterial->data.roughness = assetTexture->id;
                    break;
                  case static_cast<int>(MaterialProperty::SpecularTexture):
                    entityLitMaterial->data.specular = assetTexture->id;
                    break;
                  case static_cast<int>(MaterialProperty::EmissiveTexture):
                    entityLitMaterial->data.emissive = assetTexture->id;
                    break;
                  }
              }
            } else if (auto entitySkybox = component->As<Skybox>())
              if (assetSkybox)
                *entitySkybox = *assetSkybox;
          }
        }
        context.selectedComponent = -1;
        context.selectedProperty = -1;
      }
      if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("SPAWN_ASSET", assetName.c_str(), (assetName.size() + 1) * sizeof(char));
        ImGui::EndDragDropSource();
      }
      ImGui::PopStyleVar();
      auto textWidth = ImGui::CalcTextSize(assetName.c_str()).x;
      auto maxTextWidth = THUMBNAIL_SIZE;
      auto textX = tilePos.x + (THUMBNAIL_SIZE - std::min(textWidth, maxTextWidth)) * 0.5f;
      ImGui::SetCursorPos(ImVec2(textX, tilePos.y + THUMBNAIL_SIZE + TEXT_PADDING));
      if (textWidth > maxTextWidth) {
        ImGui::PushTextWrapPos(tilePos.x + THUMBNAIL_SIZE);
        ImGui::TextWrapped("%s", assetName.c_str());
        ImGui::PopTextWrapPos();
      } else
        ImGui::Text("%s", assetName.c_str());
      ImGui::PopID();
    }
  }
  ImGui::End();
  fileBrowser.Display();
  if (fileBrowser.HasSelected()) {
    auto filepath = fileBrowser.GetSelected();
    if (!std::filesystem::exists(filepath))
      spdlog::error("File does not exist: '{}'.", filepath.string());
    else if (fileType == FileType::Model)
      LoadModelAsync(filepath);
    else if (fileType == FileType::Image)
      LoadTextureAsync(filepath, TextureType::HDR);
    fileBrowser.ClearSelected();
    fileType = FileType::None;
  }
}
void Editor::DisplayEntity(ID id) {
  static constexpr auto INPUT_TEXT_FLAGS = ImGuiTreeNodeFlags(ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
  static constexpr auto NAME_LENGTH = 256;
  static auto renameMode = false;
  static auto entityBeingRenamed = ID::Invalid();
  static char newName[NAME_LENGTH] = "";
  auto nodeFlags = ImGuiTreeNodeFlags(ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_NavLeftJumpsToParent);
  if (!EntityHasChildren(id))
    nodeFlags |= ImGuiTreeNodeFlags_Leaf;
  if (selection.find(id) != selection.end())
    nodeFlags |= ImGuiTreeNodeFlags_Selected;
  if (renameMode && entityBeingRenamed == id) {
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
      renameMode = false;
      entityBeingRenamed = ID::Invalid();
    }
    ImGui::PopItemWidth();
    return;
  }
  auto entityName = GetEntityName(id);
  auto name = entityName.c_str();
  auto nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)id, nodeFlags, "%s", name);
  displayedEntities.push_back(id);
  auto const hovered = ImGui::IsItemHovered();
  auto const focused = ImGui::IsItemFocused();
  auto const clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
  auto const doubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
  if (clicked) {
    if (shiftHeld) {
      auto it1 = std::find(displayedEntities.begin(), displayedEntities.end(), context.selectedEntity);
      auto it2 = std::find(displayedEntities.begin(), displayedEntities.end(), id);
      if (it1 != displayedEntities.end() && it2 != displayedEntities.end()) {
        if (it1 > it2)
          // if it1 is below it2 in the list, swap them
          std::swap(it1, it2);
        if (!ctrlHeld)
          // if CTRL is pressed, append range to selection; otherwise, replace selection with range
          selection.clear();
        for (auto it = it1; it <= it2; ++it)
          // add items between the two entities (inclusive) to selection
          selection.insert(*it);
      }
    } else if (ctrlHeld) {
      if (selection.contains(id)) {
        selection.erase(id);
        context.selectedEntity = ID::Invalid();
      } else
        selection.insert(id);
    } else {
      selection.clear();
      selection.insert(id);
    }
    context.selectedEntity = id;
  } else if (focused && (shiftHeld || ctrlHeld)) {
    selection.insert(id);
    context.selectedEntity = id;
  } else if (focused && (enterPressed || spacePressed)) {
    selection.clear();
    context.selectedEntity = id;
  }
  if (hovered && doubleClicked) {
    strncpy(newName, name, NAME_LENGTH);
    newName[NAME_LENGTH - 1] = '\0';
    renameMode = true;
    entityBeingRenamed = id;
  }
  if (nodeOpen) {
    ForEachChildEntity(id, [&](ID childId) {
      DisplayEntity(childId);
    });
    ImGui::TreePop();
  }
}
void Editor::DisplayHierarchy() {
  auto& io = ImGui::GetIO();
  ctrlHeld = io.KeyCtrl;
  shiftHeld = io.KeyShift;
  backspacePressed = ImGui::IsKeyPressed(ImGuiKey_Backspace);
  deletePressed = ImGui::IsKeyPressed(ImGuiKey_Delete);
  enterPressed = ImGui::IsKeyPressed(ImGuiKey_Enter);
  escapePressed = ImGui::IsKeyPressed(ImGuiKey_Escape);
  spacePressed = ImGui::IsKeyPressed(ImGuiKey_Space);
  ImGui::Begin("Hierarchy");
  const auto focused = ImGui::IsWindowFocused();
  const auto clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  const auto windowHovered = ImGui::IsWindowHovered();
  const auto itemsHovered = ImGui::IsAnyItemHovered();
  auto clearSelection = backspacePressed || deletePressed || escapePressed || (clicked && windowHovered && !itemsHovered);
  if (deletePressed)
    for (const auto& entityId : selection)
      DeleteEntity(entityId);
  if (focused && clearSelection) {
    selection.clear();
    context.selectedEntity = ID::Invalid();
    context.assetMask = -1;
  }
  displayedEntities.clear();
  ForEachRootEntity([this](ID id) {
    DisplayEntity(id);
  });
  if (ImGui::BeginPopupContextWindow("CreateMenu", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
    if (ImGui::BeginMenu("Create")) {
      if (ImGui::MenuItem("Empty")) {
        std::string name = "Entity";
        CreateEntity(name);
        spdlog::info("Created a new entity.");
        ImGui::CloseCurrentPopup();
      }
      if (ImGui::BeginMenu("Asset")) {
        ForEachRootAsset([this](ID id) {
          auto name = GetAssetName(id);
          if (ImGui::MenuItem(name.c_str())) {
            Instantiate(name);
            spdlog::info("Created an entity from the '{}' asset.", name);
            ImGui::CloseCurrentPopup();
          }
        });
        ImGui::EndMenu();
      }
      ImGui::EndMenu();
    }
    ImGui::EndPopup();
  }
  ImGui::End();
  DisplayComponents();
}
void Editor::DisplayProperties(IComponent* component) {
  if (!component)
    return;
  if (auto camera = component->As<Camera>())
    DisplayTraits<Camera>::DisplayProperties(camera, context);
  else if (auto light = component->As<Light>())
    DisplayTraits<Light>::DisplayProperties(light, context);
  else if (auto material = component->As<Material>())
    DisplayTraits<Material>::DisplayProperties(material, context);
  else if (auto mesh = component->As<Mesh>())
    DisplayTraits<Mesh>::DisplayProperties(mesh, context);
  else if (auto filter = component->As<MeshFilter>())
    DisplayTraits<MeshFilter>::DisplayProperties(filter, context);
  else if (auto renderer = component->As<MeshRenderer>())
    DisplayTraits<MeshRenderer>::DisplayProperties(renderer, context);
  else if (auto skybox = component->As<Skybox>())
    DisplayTraits<Skybox>::DisplayProperties(skybox, context);
  else if (auto texture = component->As<Texture>())
    DisplayTraits<Texture>::DisplayProperties(texture, context);
  else if (auto transform = component->As<Transform>())
    DisplayTraits<Transform>::DisplayProperties(transform, context);
}
void Editor::DrawManipulator(float width, float height) {
  if (!context.selectedEntity.IsValid())
    return;
  auto windowPos = ImGui::GetWindowPos();
  ImGuizmo::SetRect(windowPos.x, windowPos.y, width, height);
  ImGuizmo::SetOrthographic(cameraController->camera.type == CameraType::Orthographic);
  // TODO: for a multi-select, position the gizmo at the center of the selection, and apply manipulations to all entities
  Transform transform;
  auto transformComp = GetEntityComponent<Transform>(context.selectedEntity);
  Camera* cameraComp = nullptr;
  Light* lightComp = nullptr;
  if (!transformComp) {
    cameraComp = GetEntityComponent<Camera>(context.selectedEntity);
    if (cameraComp)
      transform = cameraComp->GetTransform();
    else {
      lightComp = GetEntityComponent<Light>(context.selectedEntity);
      if (lightComp)
        transform = lightComp->GetTransform();
      else
        return;
    }
  } else
    transform = *transformComp;
  ImGuizmo::SetDrawlist();
  if (!ImGuizmo::Manipulate(glm::value_ptr(cameraController->camera.transform.view), glm::value_ptr(cameraController->camera.transform.projection), ImGuizmo::OPERATION::UNIVERSAL, ImGuizmo::MODE::WORLD, glm::value_ptr(transform.local)))
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
void Editor::ToggleGizmo(GizmoType type) {
  if (type == GizmoType::Manipulator)
    context.gizmoMask ^= static_cast<unsigned int>(GizmoMask::Manipulator);
  else if (type == GizmoType::ViewFrustum)
    context.gizmoMask ^= static_cast<unsigned int>(GizmoMask::ViewFrustum);
  else if (type == GizmoType::FrustumCulling)
    context.gizmoMask ^= static_cast<unsigned int>(GizmoMask::FrustumCulling);
}
ID Editor::Instantiate(std::string& name, const ID parentId, glm::vec3 position, glm::quat rotation, glm::vec3 scale) {
  const auto assetId = GetAssetId(name);
  if (!assetId.IsValid())
    return ID::Invalid();
  const auto entityId = CreateEntity(name);
  const auto [transform, mesh, material, boneData] = GetAssetComponents<Transform, Mesh, Material, BoneData>(assetId);
  if (transform) {
    auto entityTransform = AddEntityComponent<Transform>(entityId);
    if (!parentId.IsValid()) {
      // NOTE: child entities must retain their relative transform to parent, assign given values only if no parent
      entityTransform->position = position;
      entityTransform->rotation = rotation;
      entityTransform->scale = scale;
    } else {
      *entityTransform = *transform;
      AddChildEntity(parentId, entityId);
    }
  }
  if (mesh) {
    auto filter = AddEntityComponent<MeshFilter>(entityId);
    filter->mesh = *mesh;
  }
  if (material) {
    auto renderer = AddEntityComponent<MeshRenderer>(entityId);
    renderer->material = *material;
  }
  if (boneData) {
    auto bones = AddEntityComponent<BoneData>(entityId);
    *bones = *boneData;
  }
  ForEachChildAsset(assetId, [this, entityId](const ID childAssetId) {
    auto name = GetAssetName(childAssetId);
    Instantiate(name, entityId);
  });
  return entityId;
}
void Editor::InstantiateRandom(const std::string& name, size_t count, float radius) {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  for (auto i = 0; i < count; ++i) {
    std::uniform_real_distribution<float> dist(0.f, 1.f);
    auto theta = dist(gen) * 2.f * glm::pi<float>();
    auto phi = acos(2.f * dist(gen) - 1.f);
    auto u = dist(gen);
    auto x = sin(phi) * cos(theta);
    auto y = sin(phi) * sin(theta);
    auto z = cos(phi);
    glm::vec3 direction(x, y, z);
    auto r = radius * std::cbrt(u);
    auto position = direction * r;
    auto nameTemp = name;
    Instantiate(nameTemp, ID::Invalid(), position);
  }
}
void Editor::DisplayLogs() {
  if (!imguiSink)
    return;
  ImGui::Begin("Logs");
  imguiSink->ForEachLine([](const std::string& line) { ImGui::TextUnformatted(line.c_str()); });
  ImGui::End();
}
