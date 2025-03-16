#include <editor.hpp>
#include <entity_manager.hpp>
#include <imgui.h>
#include <scene.hpp>
#include <string>
void Editor::DisplayHierarchy() {
  ImGui::Begin("Hierarchy");
  if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && GetButton(GLFW_MOUSE_BUTTON_LEFT)) {
    selectedEntity = -1;
    clickedEntity = -1;
  }
  if (entityToDelete >= 0) {
    DeleteEntity(entityToDelete);
    if (selectedEntity == entityToDelete)
      selectedEntity = -1;
    entityToDelete = -1;
  }
  ForAllEntities([&](unsigned int id) {
    if (!EntityHasParent(id))
      DisplayEntity(id);
  });
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
  if (selectedEntity >= 0)
    DisplayProperties();
}
