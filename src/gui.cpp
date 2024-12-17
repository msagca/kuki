#include <entity_manager.hpp>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <primitive.hpp>
#include <mesh.hpp>
#include <gui.hpp>
static const auto HINT_OFFSET = 10.0f;
void ShowHints(float windowWidth, float windowHeight) {
  ImVec2 position(HINT_OFFSET, windowHeight - HINT_OFFSET);
  ImVec2 size(windowWidth - 2 * HINT_OFFSET, .0f);
  ImGui::SetNextWindowPos(position, ImGuiCond_Always, ImVec2(.0f, 1.0f));
  ImGui::SetNextWindowSize(size);
  ImGui::SetNextWindowBgAlpha(0.5f);
  ImGui::Begin("Hints", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
  for (const auto& hint : hints)
    if (hint.condition())
      ImGui::TextWrapped("%s", hint.text.c_str());
  ImGui::End();
}
void ShowCreateMenu(EntityManager& entityManager) {
  static const char* primitives[] = {"Cube"};
  static auto selectedPrimitive = -1;
  ImGui::Begin("Create");
  if (ImGui::ListBox("Primitives", &selectedPrimitive, primitives, IM_ARRAYSIZE(primitives))) {
    auto id = entityManager.CreateEntity();
    std::vector<float> vertices;
    switch (selectedPrimitive) {
    default:
      vertices = Cube::GetVerticesWithNormals();
    }
    entityManager.AddComponent<Transform>(id);
    entityManager.AddComponent<MeshRenderer>(id);
    auto& filter = entityManager.AddComponent<MeshFilter>(id);
    filter = Mesh::Create(vertices);
    selectedPrimitive = -1;
  }
  ImGui::End();
}
static void ShowProperties(EntityManager& entityManager, unsigned int selectedEntity, bool first) {
  ImGui::Begin("Properties");
  static IComponent* selectedComponent = nullptr;
  static std::unordered_map<std::string, std::variant<int, unsigned int, float, bool, glm::vec3, LightType>> propertyMap;
  auto components = entityManager.GetAllComponents(selectedEntity);
  for (const auto& component : components) {
    auto isSelected = (selectedComponent == component);
    if (ImGui::Selectable(component->GetName().c_str(), isSelected)) {
      if (isSelected)
        selectedComponent = nullptr;
      else
        selectedComponent = entityManager.GetComponent(selectedEntity, component->GetName());
    }
    auto properties = component->GetProperties();
    for (auto& prop : properties) {
      auto value = prop.value;
      if (propertyMap.find(prop.name) != propertyMap.end() && !first)
        value = propertyMap[prop.name];
      auto isColor = (prop.name == "Ambient" || prop.name == "Diffuse" || prop.name == "Specular");
      if (std::holds_alternative<glm::vec3>(value)) {
        auto valueVec3 = std::get<glm::vec3>(value);
        if (isColor) {
          if (ImGui::ColorEdit3(prop.name.c_str(), glm::value_ptr(valueVec3))) {
            propertyMap[prop.name] = valueVec3;
            component->SetProperty(Property(prop.name, valueVec3));
          }
        } else if (ImGui::InputFloat3(prop.name.c_str(), glm::value_ptr(valueVec3))) {
          propertyMap[prop.name] = valueVec3;
          component->SetProperty(Property(prop.name, valueVec3));
        }
      } else if (std::holds_alternative<int>(value)) {
        auto valueInt = std::get<int>(value);
        if (ImGui::InputInt(prop.name.c_str(), &valueInt)) {
          propertyMap[prop.name] = valueInt;
          component->SetProperty(Property(prop.name, valueInt));
        }
      } else if (std::holds_alternative<unsigned int>(value)) {
        auto valueInt = static_cast<int>(std::get<unsigned int>(value));
        if (ImGui::InputInt(prop.name.c_str(), &valueInt)) {
          propertyMap[prop.name] = valueInt;
          component->SetProperty(Property(prop.name, valueInt));
        }
      } else if (std::holds_alternative<float>(value)) {
        auto valueFloat = std::get<float>(value);
        if (ImGui::InputFloat(prop.name.c_str(), &valueFloat)) {
          propertyMap[prop.name] = valueFloat;
          component->SetProperty(Property(prop.name, valueFloat));
        }
      } else if (std::holds_alternative<bool>(value)) {
        auto valueBool = std::get<bool>(value);
        if (ImGui::Checkbox(prop.name.c_str(), &valueBool)) {
          propertyMap[prop.name] = valueBool;
          component->SetProperty(Property(prop.name, valueBool));
        }
      } else if (std::holds_alternative<LightType>(value)) {
        auto valueEnum = std::get<LightType>(value);
        static const char* items[] = {"Directional", "Point"};
        auto currentItem = static_cast<int>(valueEnum);
        if (ImGui::Combo(prop.name.c_str(), &currentItem, items, IM_ARRAYSIZE(items))) {
          valueEnum = static_cast<LightType>(currentItem);
          propertyMap[prop.name] = valueEnum;
          component->SetProperty(Property(prop.name, valueEnum));
        }
      }
    }
    ImGui::Separator();
  }
  if (selectedComponent)
    if (ImGui::Button("Remove")) {
      entityManager.RemoveComponent(selectedEntity, selectedComponent->GetName());
      selectedComponent = nullptr;
    }
  auto availableComponents = entityManager.GetMissingComponents(selectedEntity);
  for (const auto& comp : availableComponents)
    if (ImGui::Selectable(comp.c_str()))
      entityManager.AddComponent(selectedEntity, comp);
  ImGui::End();
}
void ShowHierarchyWindow(EntityManager& entityManager, InputManager& inputManager) {
  static auto selectedEntity = -1;
  static auto selectedEntityLast = -1;
  static auto renameMode = false;
  static char newName[256] = "";
  ImGui::Begin("Hierarchy");
  entityManager.ForAll([&](unsigned int id) {
    if (ImGui::Selectable(entityManager.GetName(id).c_str(), id == selectedEntity)) {
      selectedEntity = id;
      ShowProperties(entityManager, selectedEntity, true);
      selectedEntityLast = selectedEntity;
    }
  });
  auto windowHeight = ImGui::GetWindowHeight();
  auto buttonHeight = ImGui::GetFrameHeight();
  auto padding = ImGui::GetStyle().WindowPadding.y;
  ImGui::SetCursorPosY(windowHeight - buttonHeight - padding);
  if (ImGui::Button("New")) {
    entityManager.CreateEntity();
    selectedEntity = -1;
  }
  if (selectedEntity != -1) {
    ImGui::SameLine();
    if (ImGui::Button("Remove")) {
      entityManager.RemoveEntity(selectedEntity);
      selectedEntity = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Rename")) {
      renameMode = true;
      strcpy(newName, entityManager.GetName(selectedEntity).c_str());
    }
  }
  ImGui::End();
  if (renameMode) {
    inputManager.DisableKeyCallbacks();
    ImGui::Begin("Rename Entity", &renameMode);
    if (ImGui::InputText("New Name", newName, IM_ARRAYSIZE(newName), ImGuiInputTextFlags_EnterReturnsTrue)) {
      entityManager.RenameEntity(selectedEntity, std::string(newName));
      renameMode = false;
    }
    if (!renameMode)
      inputManager.EnableKeyCallbacks();
    ImGui::End();
  }
  if (selectedEntity != -1)
    ShowProperties(entityManager, selectedEntity, selectedEntity != selectedEntityLast);
}
