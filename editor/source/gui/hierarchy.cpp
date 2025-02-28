#include <application.hpp>
#include <editor.hpp>
#include <entity_manager.hpp>
#include <imgui.h>
#include <scene.hpp>
#include <string>
void Editor::DisplayHierarchy() {
  auto scene = GetActiveScene();
  auto& entityManager = scene->GetEntityManager();
  auto& spawnManager = scene->GetSpawnManager();
  ImGui::Begin("Hierarchy");
  if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
    selectedEntity = -1;
    clickedEntity = -1;
  }
  if (entityToDelete >= 0) {
    entityManager.Delete(entityToDelete);
    if (selectedEntity == entityToDelete)
      selectedEntity = -1;
    entityToDelete = -1;
  }
  entityManager.ForAll([&](unsigned int id) {
    if (!entityManager.HasParent(id))
      DisplayEntity(id, entityManager);
  });
  if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(1))
    ImGui::OpenPopup("CreateMenu");
  if (ImGui::BeginPopup("CreateMenu")) {
    if (ImGui::BeginMenu("Create")) {
      if (ImGui::MenuItem("Empty")) {
        std::string name = "Entity";
        entityManager.Create(name);
        ImGui::CloseCurrentPopup();
      }
      if (ImGui::BeginMenu("Asset")) {
        assetManager.ForEachRoot([this, &scene, &spawnManager](unsigned int id) {
          auto name = assetManager.GetName(id);
          if (ImGui::MenuItem(name.c_str())) {
            spawnManager.Spawn(name);
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
    DisplayProperties(selectedEntity);
}
