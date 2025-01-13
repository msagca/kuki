#include <component_types.hpp>
#include <cstring>
#include <entity_manager.hpp>
#include <functional>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <gui.hpp>
#include <imgui.h>
#include <input_manager.hpp>
#include <string>
#include <variant>
static const auto IMGUI_NON_INTERACTABLE_FLAGS = ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;
static const auto IMGUI_WINDOW_FLAGS = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize;
void DisplayFPS(unsigned int fps) {
  ImVec2 windowPos(ImGui::GetIO().DisplaySize.x / 2, 0);
  ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(1.0f, .0f));
  ImGui::SetNextWindowBgAlpha(.5f);
  ImGui::Begin("FPS", nullptr, IMGUI_NON_INTERACTABLE_FLAGS);
  ImGui::Text("%u", fps);
  ImGui::End();
}
static void ShowProperties(EntityManager& entityManager, unsigned int entityID) {
  ImGui::Begin("Properties", nullptr, IMGUI_WINDOW_FLAGS);
  auto contentRegion = ImGui::GetContentRegionAvail();
  auto windowPos = ImGui::GetWindowPos();
  windowPos.x = ImGui::GetIO().DisplaySize.x - contentRegion.x;
  windowPos.y = 0;
  ImGui::SetWindowPos(windowPos);
  static IComponent* selection = nullptr;
  auto components = entityManager.GetAllComponents(entityID);
  for (auto i = 0; i < components.size(); ++i) {
    const auto& component = components[i];
    auto isSelected = (selection == component);
    ImGui::PushID(static_cast<int>(i));
    if (ImGui::Selectable(component->GetName().c_str(), isSelected)) {
      if (isSelected)
        selection = nullptr;
      else
        selection = entityManager.GetComponent(entityID, component->GetName());
    }
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Remove")) {
        entityManager.RemoveComponent(entityID, component->GetName());
        if (selection == component)
          selection = nullptr;
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
  auto availableComponents = entityManager.GetMissingComponents(entityID);
  for (const auto& comp : availableComponents)
    if (ImGui::Selectable(comp.c_str()))
      entityManager.AddComponent(entityID, comp);
  ImGui::End();
}
static void ShowEntityHierarchy(EntityManager& entityManager, unsigned int parentID, int& entityID) {
  entityManager.ForAll([&](unsigned int id) {
    if (entityManager.GetParent(id) == parentID) {
      ImGuiTreeNodeFlags nodeFlags = (id == entityID ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
      bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)id, nodeFlags, entityManager.GetName(id).c_str());
      if (ImGui::IsItemClicked())
        entityID = id;
      if (nodeOpen) {
        ShowEntityHierarchy(entityManager, id, entityID);
        ImGui::TreePop();
      }
    }
  });
}
void DisplayHierarchy(EntityManager& entityManager, InputManager& inputManager) {
  ImVec2 windowPos(0, 0);
  ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
  static auto selection = -1;
  static auto renameMode = false;
  static char newName[256] = "";
  static const char* primitives[] = {"Empty", "Cube", "Sphere", "Cylinder", "Backpack"};
  static int selectedPrimitive = -1;
  ImGui::Begin("Hierarchy", nullptr, IMGUI_WINDOW_FLAGS);
  entityManager.ForAll([&](unsigned int id) {
    if (!entityManager.HasParent(id)) {
      if (ImGui::Selectable(entityManager.GetName(id).c_str(), id == selection))
        selection = id;
      ShowEntityHierarchy(entityManager, id, selection);
    }
  });
  if (ImGui::BeginPopupContextWindow()) {
    if (ImGui::BeginMenu("New")) {
      if (ImGui::ListBox("Primitives", &selectedPrimitive, primitives, IM_ARRAYSIZE(primitives))) {
        if (selectedPrimitive == 0)
          entityManager.Create();
        else
          entityManager.Spawn(primitives[selectedPrimitive]);
        selection = -1;
        selectedPrimitive = -1;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndMenu();
    }
    if (selection != -1) {
      if (ImGui::MenuItem("Remove")) {
        entityManager.Remove(selection);
        selection = -1;
      }
      if (ImGui::MenuItem("Rename")) {
        renameMode = true;
        strcpy(newName, entityManager.GetName(selection).c_str());
      }
    }
    ImGui::EndPopup();
  }
  ImGui::End();
  if (renameMode) {
    inputManager.DisableKeyCallbacks();
    ImGui::Begin("Rename Entity", &renameMode);
    ImGui::SetKeyboardFocusHere();
    if (ImGui::InputText("New Name", newName, IM_ARRAYSIZE(newName), ImGuiInputTextFlags_EnterReturnsTrue)) {
      entityManager.Rename(selection, std::string(newName));
      renameMode = false;
    }
    if (!renameMode)
      inputManager.EnableKeyCallbacks();
    ImGui::End();
  }
  if (selection != -1)
    ShowProperties(entityManager, selection);
}
