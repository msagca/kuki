#define GLM_ENABLE_EXPERIMENTAL
#include <camera_controller.hpp>
#include <component_types.hpp>
#include <cstring>
#include <entity_manager.hpp>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <input_manager.hpp>
#include <string>
#include <variant>
static const auto IMGUI_NON_INTERACTABLE_FLAGS = ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;
static const auto IMGUI_WINDOW_FLAGS = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize;
void DisplayFPS(unsigned int fps) {
  ImGui::SetNextWindowBgAlpha(.5f);
  ImGui::Begin("FPS", nullptr, IMGUI_NON_INTERACTABLE_FLAGS);
  auto contentRegion = ImGui::GetContentRegionAvail();
  auto windowPos = ImGui::GetWindowPos();
  auto displaySize = ImGui::GetIO().DisplaySize;
  windowPos.x = (displaySize.x - contentRegion.x) / 2;
  windowPos.y = 0;
  ImGui::SetWindowPos(windowPos);
  ImGui::Text("%u", fps);
  ImGui::End();
}
void DisplayKeyBindings(InputManager& inputManager) {
  auto& keyBindings = inputManager.GetKeyBindings();
  ImGui::SetNextWindowBgAlpha(.5f);
  ImGui::Begin("Key Bindings", nullptr, IMGUI_NON_INTERACTABLE_FLAGS);
  auto contentRegion = ImGui::GetContentRegionAvail();
  auto windowPos = ImGui::GetWindowPos();
  auto displaySize = ImGui::GetIO().DisplaySize;
  windowPos.x = (displaySize.x - contentRegion.x) / 2;
  windowPos.y = (displaySize.y - contentRegion.y) / 2;
  ImGui::SetWindowPos(windowPos);
  for (const auto& pair : keyBindings)
    ImGui::Text("%s: %s", pair.first.c_str(), pair.second.c_str());
  ImGui::End();
}
static void DisplayProperties(EntityManager& entityManager, unsigned int selectedEntity) {
  ImGui::Begin("Properties", nullptr, IMGUI_WINDOW_FLAGS);
  auto contentRegion = ImGui::GetContentRegionAvail();
  auto windowPos = ImGui::GetWindowPos();
  windowPos.x = ImGui::GetIO().DisplaySize.x - contentRegion.x;
  windowPos.y = 0;
  ImGui::SetWindowPos(windowPos);
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
        static const char* items[] = {"DiffuseMap", "NormalMap", "SpecularMap", "HeightMap", "RoughnessMap", "MetalnessMap"};
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
static void DisplayEntityHierarchy(EntityManager& entityManager, unsigned int id, int& selectedEntity, int& clickedEntity) {
  auto name = entityManager.GetName(id).c_str();
  auto hasChildren = entityManager.HasChildren(id);
  ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | (id == selectedEntity ? ImGuiTreeNodeFlags_Selected : 0) | (hasChildren ? ImGuiTreeNodeFlags_OpenOnArrow : ImGuiTreeNodeFlags_Leaf);
  auto nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)id, nodeFlags, name);
  if (ImGui::IsItemClicked())
    selectedEntity = id;
  if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    clickedEntity = id;
  if (nodeOpen) {
    entityManager.ForEachChild(id, [&](unsigned int childID) {
      DisplayEntityHierarchy(entityManager, childID, selectedEntity, clickedEntity);
    });
    ImGui::TreePop();
  }
}
void DisplayHierarchy(EntityManager& entityManager, InputManager& inputManager, CameraController& cameraController) {
  ImVec2 windowPos(0, 0);
  ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
  static auto selectedEntity = -1;
  static auto clickedEntity = -1;
  static auto renameMode = false;
  static char newName[256] = "";
  static auto gizmoOp(ImGuizmo::OPERATION::UNIVERSAL);
  static auto gizmoMode(ImGuizmo::MODE::WORLD);
  ImGui::Begin("Hierarchy", nullptr, IMGUI_WINDOW_FLAGS);
  entityManager.ForAll([&](unsigned int id) {
    if (!entityManager.HasParent(id))
      DisplayEntityHierarchy(entityManager, id, selectedEntity, clickedEntity);
  });
  if (clickedEntity >= 0)
    if (ImGui::BeginPopupContextWindow()) {
      if (ImGui::MenuItem("Remove")) {
        entityManager.Remove(clickedEntity);
        clickedEntity = -1;
        selectedEntity = -1;
      } else if (ImGui::MenuItem("Rename")) {
        strcpy(newName, entityManager.GetName(clickedEntity).c_str());
        renameMode = true;
      }
      ImGui::EndPopup();
    }
  ImGui::End();
  if (renameMode) {
    inputManager.DisableKeyCallbacks();
    ImGui::Begin("Rename", &renameMode);
    ImGui::SetKeyboardFocusHere();
    if (ImGui::InputText("New Name", newName, IM_ARRAYSIZE(newName), ImGuiInputTextFlags_EnterReturnsTrue)) {
      entityManager.Rename(clickedEntity, std::string(newName));
      renameMode = false;
    }
    if (!renameMode)
      inputManager.EnableKeyCallbacks();
    ImGui::End();
  }
  if (selectedEntity >= 0) {
    DisplayProperties(entityManager, selectedEntity);
    auto transform = entityManager.GetComponent<Transform>(selectedEntity);
    if (transform) {
      // draw gizmo
      glm::mat4 matrix;
      auto rotation = glm::degrees(transform->rotation);
      ImGuizmo::RecomposeMatrixFromComponents(glm::value_ptr(transform->position), glm::value_ptr(rotation), glm::value_ptr(transform->scale), glm::value_ptr(matrix));
      ImGuizmo::SetOrthographic(false);
      auto displaySize = ImGui::GetIO().DisplaySize;
      ImGuizmo::SetRect(0, 0, displaySize.x, displaySize.y);
      ImGuizmo::Manipulate(glm::value_ptr(cameraController.GetView()), glm::value_ptr(cameraController.GetProjection()), gizmoOp, gizmoMode, glm::value_ptr(matrix));
      ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(matrix), glm::value_ptr(transform->position), glm::value_ptr(rotation), glm::value_ptr(transform->scale));
      transform->rotation = glm::radians(rotation);
    }
  }
}
void DisplayCreateMenu(EntityManager& entityManager, bool& showMenu) {
  static const char* primitives[] = {"Cube", "Sphere", "Cylinder"};
  static const char* models[] = {"Backpack"}; // TODO: fetch a list of models from the asset manager
  static auto selectedPrimitive = -1;
  static auto selectedModel = -1;
  ImGui::OpenPopup("Create");
  if (ImGui::BeginPopup("Create")) {
    if (ImGui::Selectable("Empty")) {
      entityManager.Create();
      ImGui::CloseCurrentPopup();
      showMenu = false;
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("Primitive")) {
      if (ImGui::ListBox("##Primitives", &selectedPrimitive, primitives, IM_ARRAYSIZE(primitives))) {
        entityManager.Spawn(primitives[selectedPrimitive]);
        selectedPrimitive = -1;
        ImGui::CloseCurrentPopup();
        showMenu = false;
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Model")) {
      if (ImGui::ListBox("##Models", &selectedModel, models, IM_ARRAYSIZE(models))) {
        entityManager.Spawn(models[selectedModel]);
        selectedModel = -1;
        ImGui::CloseCurrentPopup();
        showMenu = false;
      }
      ImGui::EndMenu();
    }
    ImGui::EndPopup();
  }
}
