#include <editor.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <string>
#include <utility>
#include <spdlog/spdlog.h>
void Editor::DisplayHierarchy() {
  ImGui::Begin("Hierarchy");
  deleteSelected |= GetKey(GLFW_KEY_DELETE);
  addRangeToSelection = GetKey(GLFW_KEY_LEFT_SHIFT);
  addToSelection = GetKey(GLFW_KEY_LEFT_CONTROL);
  if (currentSelection < lastSelection)
    std::swap(currentSelection, lastSelection);
  if (clearSelection) {
    selectedEntities.clear();
    clearSelection = false;
  }
  ForAllEntities([&](unsigned int id) {
    if (addRangeToSelection && id >= lastSelection && id <= currentSelection || (addToSelection || deleteSelected) && id == currentSelection)
      // FIXME: child entities are not displayed in a predictable order, so the selection range is usually not correct
      selectedEntities.insert(id);
    if (!EntityHasParent(id))
      DisplayEntity(id);
  });
  if (deleteSelected && !selectedEntities.empty()) {
    for (const auto id : selectedEntities)
      DeleteEntity(id);
    spdlog::info("Deleted {} entities.", selectedEntities.size());
  }
  if (deleteSelected || !flying && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && GetButton(GLFW_MOUSE_BUTTON_LEFT)) {
    lastSelection = -1;
    currentSelection = -1;
    deleteSelected = false;
    selectedEntities.clear();
  }
  if (!flying && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && GetButton(GLFW_MOUSE_BUTTON_RIGHT))
    ImGui::OpenPopup("CreateMenu");
  if (!flying && ImGui::BeginPopup("CreateMenu")) {
    if (ImGui::BeginMenu("Create")) {
      if (ImGui::MenuItem("Empty")) {
        std::string name = "Entity";
        CreateEntity(name);
        spdlog::info("Created a new entity.");
        ImGui::CloseCurrentPopup();
      }
      if (ImGui::BeginMenu("Asset")) {
        ForEachRootAsset([this](unsigned int id) {
          auto name = GetAssetName(id);
          if (ImGui::MenuItem(name.c_str())) {
            Spawn(name);
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
  DisplayProperties();
}
