#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtc/type_ptr.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <asset_loader.hpp>
#include <asset_manager.hpp>
#include <camera_controller.hpp>
#include <component/camera.hpp>
#include <component/component.hpp>
#include <component/light.hpp>
#include <cstring>
#include <editor.hpp>
#include <entity_manager.hpp>
#include <glm/ext/vector_float3.hpp>
#include <input_manager.hpp>
#include <render_system.hpp>
#include <variant>
static const auto WINDOW_FLAGS = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
Editor::Editor(AssetManager& assetManager, AssetLoader& assetLoader, EntityManager& entityManager, CameraController& cameraController, RenderSystem& renderSystem)
  : assetManager(assetManager), assetLoader(assetLoader), entityManager(entityManager), cameraController(cameraController), renderSystem(renderSystem) {
}
void Editor::Init(GLFWwindow* window) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_DockingEnable;
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();
  LoadDefaultScene();
}
void Editor::Render() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGui::DockSpaceOverViewport(ImGui::GetID("DockSpace"));
  DisplayHierarchy();
  DisplayGame();
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
void Editor::CleanUp() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
void Editor::LoadDefaultScene() {
  auto cameraID = entityManager.Create("MainCamera");
  entityManager.AddComponent<Camera>(cameraID);
  auto lightID = entityManager.Create("MainLight");
  entityManager.AddComponent<Light>(lightID);
}
void Editor::DisplayGame() {
  static const ImVec2 uv0(.0f, 1.0f);
  static const ImVec2 uv1(1.0f, .0f);
  ImGui::Begin("Game");
  auto contentRegion = ImGui::GetContentRegionAvail();
  auto texture = renderSystem.RenderToTexture(contentRegion.x, contentRegion.y, selectedEntity);
  if (texture >= 0)
    ImGui::Image(texture, ImVec2(contentRegion.x, contentRegion.y), uv0, uv1);
  ImGui::End();
}
void Editor::DisplayHierarchy() {
  static auto clickedEntity = -1;
  static auto renameMode = false;
  static char newName[256] = "";
  ImGui::Begin("Hierarchy");
  entityManager.ForAll([&](unsigned int id) {
    if (!entityManager.HasParent(id))
      DisplayEntityHierarchy(id, selectedEntity, clickedEntity);
  });
  if (ImGui::BeginPopupContextWindow()) {
    if (clickedEntity >= 0) {
      if (ImGui::MenuItem("Remove")) {
        entityManager.Remove(clickedEntity);
        clickedEntity = -1;
        selectedEntity = -1;
      } else if (ImGui::MenuItem("Rename")) {
        strcpy(newName, entityManager.GetName(clickedEntity).c_str());
        renameMode = true;
      }
    } else
      DisplayCreateMenu();
    ImGui::EndPopup();
  }
  ImGui::End();
  if (renameMode) {
    InputManager::GetInstance().DisableKeyCallbacks();
    ImGui::Begin("Rename", &renameMode);
    ImGui::SetKeyboardFocusHere();
    if (ImGui::InputText("New Name", newName, IM_ARRAYSIZE(newName), ImGuiInputTextFlags_EnterReturnsTrue)) {
      entityManager.Rename(clickedEntity, std::string(newName));
      renameMode = false;
    }
    if (!renameMode)
      InputManager::GetInstance().EnableKeyCallbacks();
    ImGui::End();
  }
  if (selectedEntity >= 0)
    DisplayProperties(selectedEntity);
}
void Editor::DisplayEntityHierarchy(unsigned int id, int& selectedEntity, int& clickedEntity) {
  auto name = entityManager.GetName(id).c_str();
  auto hasChildren = entityManager.HasChildren(id);
  ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | (id == selectedEntity ? ImGuiTreeNodeFlags_Selected : 0) | (hasChildren ? ImGuiTreeNodeFlags_OpenOnArrow : ImGuiTreeNodeFlags_Leaf);
  auto nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)id, nodeFlags, "%s", name);
  if (ImGui::IsItemClicked())
    selectedEntity = id;
  if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    clickedEntity = id;
  if (nodeOpen) {
    entityManager.ForEachChild(id, [&](unsigned int childID) {
      DisplayEntityHierarchy(childID, selectedEntity, clickedEntity);
    });
    ImGui::TreePop();
  }
}
void Editor::DisplayProperties(unsigned int selectedEntity) {
  ImGui::Begin("Properties");
  static IComponent* selectedComponent = nullptr;
  auto components = entityManager.GetAllComponents(selectedEntity);
  for (auto i = 0; i < components.size(); ++i) {
    const auto& component = components[i];
    auto isSelected = (selectedComponent == component);
    ImGui::PushID(static_cast<int>(i));
    if (ImGui::Selectable(component->GetName().c_str(), isSelected)) {
      if (isSelected)
        selectedComponent = nullptr;
      else
        selectedComponent = entityManager.GetComponent(selectedEntity, component->GetName());
    }
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Remove")) {
        entityManager.RemoveComponent(selectedEntity, component->GetName());
        if (selectedComponent == component)
          selectedComponent = nullptr;
      }
      ImGui::EndPopup();
    }
    auto properties = component->GetProperties();
    for (auto j = 0; j < properties.size(); ++j) {
      auto& prop = properties[j];
      auto value = prop.value;
      ImGui::PushID(static_cast<int>(j));
      auto isColor = (prop.name == "Ambient" || prop.name == "Diffuse" || prop.name == "Specular");
      if (std::holds_alternative<glm::vec3>(value)) {
        auto valueVec3 = std::get<glm::vec3>(value);
        if (isColor) {
          if (ImGui::ColorEdit3(prop.name.c_str(), glm::value_ptr(valueVec3)))
            component->SetProperty(Property(prop.name, valueVec3));
        } else if (ImGui::InputFloat3(prop.name.c_str(), glm::value_ptr(valueVec3)))
          component->SetProperty(Property(prop.name, valueVec3));
      } else if (std::holds_alternative<int>(value)) {
        auto valueInt = std::get<int>(value);
        if (ImGui::InputInt(prop.name.c_str(), &valueInt))
          component->SetProperty(Property(prop.name, valueInt));
      } else if (std::holds_alternative<unsigned int>(value)) {
        auto valueInt = static_cast<int>(std::get<unsigned int>(value));
        if (ImGui::InputInt(prop.name.c_str(), &valueInt))
          component->SetProperty(Property(prop.name, valueInt));
      } else if (std::holds_alternative<float>(value)) {
        auto valueFloat = std::get<float>(value);
        if (ImGui::InputFloat(prop.name.c_str(), &valueFloat))
          component->SetProperty(Property(prop.name, valueFloat));
      } else if (std::holds_alternative<bool>(value)) {
        auto valueBool = std::get<bool>(value);
        if (ImGui::Checkbox(prop.name.c_str(), &valueBool))
          component->SetProperty(Property(prop.name, valueBool));
      } else if (std::holds_alternative<CameraType>(value)) {
        auto valueEnum = std::get<CameraType>(value);
        static const char* items[] = {"Perspective", "Orthographic"};
        auto currentItem = static_cast<int>(valueEnum);
        if (ImGui::Combo(prop.name.c_str(), &currentItem, items, IM_ARRAYSIZE(items))) {
          valueEnum = static_cast<CameraType>(currentItem);
          component->SetProperty(Property(prop.name, valueEnum));
        }
      } else if (std::holds_alternative<LightType>(value)) {
        auto valueEnum = std::get<LightType>(value);
        static const char* items[] = {"Directional", "Point"};
        auto currentItem = static_cast<int>(valueEnum);
        if (ImGui::Combo(prop.name.c_str(), &currentItem, items, IM_ARRAYSIZE(items))) {
          valueEnum = static_cast<LightType>(currentItem);
          component->SetProperty(Property(prop.name, valueEnum));
        }
      } else if (std::holds_alternative<TextureType>(value)) {
        auto valueEnum = std::get<TextureType>(value);
        static const char* items[] = {"Base", "Normal", "ORM", "Metalness", "Occlusion", "Roughness"};
        auto currentItem = static_cast<int>(valueEnum);
        if (ImGui::Combo(prop.name.c_str(), &currentItem, items, IM_ARRAYSIZE(items))) {
          valueEnum = static_cast<TextureType>(currentItem);
          component->SetProperty(Property(prop.name, valueEnum));
        }
      }
      ImGui::PopID();
    }
    ImGui::PopID();
  }
  auto availableComponents = entityManager.GetMissingComponents(selectedEntity);
  for (const auto& comp : availableComponents)
    if (ImGui::Selectable(comp.c_str()))
      entityManager.AddComponent(selectedEntity, comp);
  ImGui::End();
}
void Editor::DisplayCreateMenu() {
  static const char* primitives[] = {"Cube", "Sphere", "Cylinder"};
  static auto selectedPrimitive = -1;
  ImGui::OpenPopup("Create");
  if (ImGui::BeginPopup("Create")) {
    if (ImGui::Selectable("Empty")) {
      entityManager.Create();
      ImGui::CloseCurrentPopup();
    }
    if (ImGui::BeginMenu("Primitive")) {
      if (ImGui::ListBox("##Primitives", &selectedPrimitive, primitives, IM_ARRAYSIZE(primitives))) {
        entityManager.Spawn(primitives[selectedPrimitive]);
        selectedPrimitive = -1;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndMenu();
    }
    ImGui::EndPopup();
  }
}
