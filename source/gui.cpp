#include <component_types.hpp>
#include <asset_manager.hpp>
#include <entity_manager.hpp>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <primitive.hpp>
#include <gui.hpp>
static const auto SCREEN_EDGE_OFFSET = 10.0f;
static const auto IMGUI_NON_INTERACTABLE_FLAGS = ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;
void ShowHints(float windowWidth, float windowHeight) {
  ImVec2 position(SCREEN_EDGE_OFFSET, windowHeight - SCREEN_EDGE_OFFSET);
  ImVec2 size(windowWidth - 2 * SCREEN_EDGE_OFFSET, .0f);
  ImGui::SetNextWindowPos(position, ImGuiCond_Always, ImVec2(.0f, 1.0f));
  ImGui::SetNextWindowSize(size);
  ImGui::SetNextWindowBgAlpha(0.5f);
  ImGui::Begin("Hints", nullptr, IMGUI_NON_INTERACTABLE_FLAGS);
  for (const auto& hint : hints)
    if (hint.condition())
      ImGui::TextWrapped("%s", hint.text.c_str());
  ImGui::End();
}
void ShowFPS(unsigned int fps, float windowWidth) {
  ImVec2 position(windowWidth - SCREEN_EDGE_OFFSET, SCREEN_EDGE_OFFSET);
  ImGui::SetNextWindowPos(position, ImGuiCond_Always, ImVec2(1.0f, .0f));
  ImGui::SetNextWindowBgAlpha(.5f);
  ImGui::Begin("FPS", nullptr, IMGUI_NON_INTERACTABLE_FLAGS);
  ImGui::Text("%u", fps);
  ImGui::End();
}
void ShowCreateMenu(EntityManager& entityManager, AssetManager& assetManager) {
  static const char* primitives[] = {"Cube", "Sphere", "Cylinder"};
  static auto primID = -1;
  ImGui::Begin("Create");
  if (ImGui::ListBox("Primitives", &primID, primitives, IM_ARRAYSIZE(primitives))) {
    auto entityID = entityManager.Create(primitives[primID]);
    std::vector<Vertex> vertices;
    switch (primID) {
    case 0:
      vertices = Primitive::Cube();
      break;
    case 1:
      vertices = Primitive::Sphere();
      break;
    default:
      vertices = Primitive::Cylinder();
    }
    entityManager.AddComponent<Transform>(entityID);
    entityManager.AddComponent<MeshRenderer>(entityID);
    auto& filter = entityManager.AddComponent<MeshFilter>(entityID);
    auto assetID = assetManager.Create(primitives[primID]);
    auto& mesh = assetManager.AddComponent<Mesh>(assetID);
    mesh = assetManager.CreateMesh(vertices);
    filter.mesh = mesh;
    primID = -1;
  }
  ImGui::End();
}
static void ShowProperties(EntityManager& entityManager, unsigned int selectedEntity, bool first) {
  ImGui::Begin("Properties");
  static IComponent* componentID = nullptr;
  auto components = entityManager.GetAllComponents(selectedEntity);
  for (const auto& component : components) {
    auto isSelected = (componentID == component);
    if (ImGui::Selectable(component->GetName().c_str(), isSelected)) {
      if (isSelected)
        componentID = nullptr;
      else
        componentID = entityManager.GetComponent(selectedEntity, component->GetName());
    }
    auto properties = component->GetProperties();
    for (auto& prop : properties) {
      auto value = prop.value;
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
      }
    }
    ImGui::Separator();
  }
  if (componentID)
    if (ImGui::Button("Remove")) {
      entityManager.RemoveComponent(selectedEntity, componentID->GetName());
      componentID = nullptr;
    }
  auto availableComponents = entityManager.GetMissingComponents(selectedEntity);
  for (const auto& comp : availableComponents)
    if (ImGui::Selectable(comp.c_str()))
      entityManager.AddComponent(selectedEntity, comp);
  ImGui::End();
}
void ShowHierarchyWindow(EntityManager& entityManager, InputManager& inputManager) {
  static auto entityID = -1;
  static auto entityIDLast = -1;
  static auto renameMode = false;
  static char newName[256] = "";
  ImGui::Begin("Hierarchy");
  entityManager.ForAll([&](unsigned int id) {
    if (ImGui::Selectable(entityManager.GetName(id).c_str(), id == entityID)) {
      entityID = id;
      ShowProperties(entityManager, entityID, true);
      entityIDLast = entityID;
    }
  });
  auto windowHeight = ImGui::GetWindowHeight();
  auto buttonHeight = ImGui::GetFrameHeight();
  auto padding = ImGui::GetStyle().WindowPadding.y;
  ImGui::SetCursorPosY(windowHeight - buttonHeight - padding);
  if (ImGui::Button("New")) {
    entityManager.Create();
    entityID = -1;
  }
  if (entityID != -1) {
    ImGui::SameLine();
    if (ImGui::Button("Remove")) {
      entityManager.Remove(entityID);
      entityID = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Rename")) {
      renameMode = true;
      strcpy(newName, entityManager.GetName(entityID).c_str());
    }
  }
  ImGui::End();
  if (renameMode) {
    inputManager.DisableKeyCallbacks();
    ImGui::Begin("Rename Entity", &renameMode);
    if (ImGui::InputText("New Name", newName, IM_ARRAYSIZE(newName), ImGuiInputTextFlags_EnterReturnsTrue)) {
      entityManager.Rename(entityID, std::string(newName));
      renameMode = false;
    }
    if (!renameMode)
      inputManager.EnableKeyCallbacks();
    ImGui::End();
  }
  if (entityID != -1)
    ShowProperties(entityManager, entityID, entityID != entityIDLast);
}
