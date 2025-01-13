#include <component_types.hpp>
#include <cstring>
#include <entity_manager.hpp>
#include <functional>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/type_ptr.hpp>
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
static void DisplayEntityHierarchy(EntityManager& entityManager, unsigned int parentID, int& selectedEntity, int& clickedEntity) {
  entityManager.ForAll([&](unsigned int id) {
    if (entityManager.GetParent(id) == parentID) {
      auto hasChildren = entityManager.HasChildren(id);
      ImGuiTreeNodeFlags nodeFlags = (id == selectedEntity ? ImGuiTreeNodeFlags_Selected : 0) | (hasChildren ? ImGuiTreeNodeFlags_OpenOnArrow : ImGuiTreeNodeFlags_Leaf);
      bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)id, nodeFlags, entityManager.GetName(id).c_str());
      if (ImGui::IsItemClicked())
        selectedEntity = id;
      if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        clickedEntity = id;
      if (nodeOpen) {
        if (hasChildren)
          DisplayEntityHierarchy(entityManager, id, selectedEntity, clickedEntity);
        ImGui::TreePop();
      }
    }
  });
}
void DisplayHierarchy(EntityManager& entityManager, InputManager& inputManager) {
  ImVec2 windowPos(0, 0);
  ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
  static auto selectedEntity = -1;
  static auto clickedEntity = -1;
  static auto renameMode = false;
  static char newName[256] = "";
  static const char* primitives[] = {"Empty", "Cube", "Sphere", "Cylinder"};
  static int selectedPrimitive = -1;
  static const char* models[] = {"Backpack"}; // TODO: fetch a list of models from the asset manager
  static int selectedModel = -1;
  ImGui::Begin("Hierarchy", nullptr, IMGUI_WINDOW_FLAGS);
  entityManager.ForAll([&](unsigned int id) {
    if (!entityManager.HasParent(id)) {
      if (ImGui::Selectable(entityManager.GetName(id).c_str(), id == selectedEntity))
        selectedEntity = id;
      if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        clickedEntity = id;
      DisplayEntityHierarchy(entityManager, id, selectedEntity, clickedEntity);
    }
  });
  if (clickedEntity >= 0)
    if (ImGui::BeginPopupContextWindow()) {
      if (ImGui::MenuItem("Remove")) {
        entityManager.Remove(clickedEntity);
        clickedEntity = -1;
        selectedEntity = -1;
      } else if (ImGui::MenuItem("Rename")) {
        // save entity name to display in the input field later
        strcpy(newName, entityManager.GetName(clickedEntity).c_str());
        renameMode = true;
      }
      ImGui::EndPopup();
    }
  ImGui::End();
  if (ImGui::IsKeyPressed(ImGuiKey_Space))
    ImGui::OpenPopup("Create");
  if (ImGui::BeginPopup("Create")) {
    if (ImGui::BeginMenu("Primitive")) {
      if (ImGui::ListBox("##Primitives", &selectedPrimitive, primitives, IM_ARRAYSIZE(primitives))) {
        if (selectedPrimitive == 0)
          entityManager.Create();
        else
          entityManager.Spawn(primitives[selectedPrimitive]);
        selectedPrimitive = -1;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Model")) {
      if (ImGui::ListBox("##Models", &selectedModel, models, IM_ARRAYSIZE(models))) {
        entityManager.Spawn(models[selectedModel]);
        selectedModel = -1;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndMenu();
    }
    ImGui::EndPopup();
  }
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
  if (selectedEntity >= 0)
    DisplayProperties(entityManager, selectedEntity);
}
