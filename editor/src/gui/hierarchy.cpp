#include <editor.hpp>
#include <entity_manager.hpp>
#include <imgui.h>
#include <scene.hpp>
#include <string>
void Editor::DisplayHierarchy() {
  ImGui::Begin("Hierarchy");
  auto deleteSelected = GetKey(GLFW_KEY_DELETE);
  if (currentSelection < lastSelection)
    std::swap(currentSelection, lastSelection);
  static std::unordered_set<unsigned int> selectedEntities;
  ForAllEntities([&](unsigned int id) {
    if (!EntityHasParent(id)) {
      if (deleteSelected && id >= lastSelection && id <= currentSelection)
        selectedEntities.insert(id);
      DisplayEntity(id);
    }
  });
  if (deleteSelected && !selectedEntities.empty())
    for (const auto id : selectedEntities)
      DeleteEntity(id);
  if (deleteSelected || ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && GetButton(GLFW_MOUSE_BUTTON_LEFT)) {
    lastSelection = -1;
    currentSelection = -1;
    selectedEntities.clear();
  }
  if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && GetButton(GLFW_MOUSE_BUTTON_RIGHT))
    ImGui::OpenPopup("CreateMenu");
  if (ImGui::BeginPopup("CreateMenu")) {
    if (ImGui::BeginMenu("Create")) {
      if (ImGui::MenuItem("Empty")) {
        std::string name = "Entity";
        CreateEntity(name);
        ImGui::CloseCurrentPopup();
      }
      if (ImGui::BeginMenu("Asset")) {
        ForEachRootAsset([this](unsigned int id) {
          auto name = GetAssetName(id);
          if (ImGui::MenuItem(name.c_str())) {
            Spawn(name);
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
  DisplayProperties();
}
