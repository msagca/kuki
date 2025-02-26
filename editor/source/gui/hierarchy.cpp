#include <application.hpp>
#include <editor.hpp>
#include <entity_manager.hpp>
#include <imgui.h>
#include <scene.hpp>
#include <string>
void Editor::DisplayHierarchy() {
  static const char* primitives[] = {"Cube", "Sphere", "Cylinder"};
  auto scene = GetActiveScene();
  auto& entityManager = scene->GetEntityManager();
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
        scene->GetEntityManager().Create(name);
        ImGui::CloseCurrentPopup();
      }
      if (ImGui::BeginMenu("Primitive")) {
        for (auto i = 0; i < IM_ARRAYSIZE(primitives); ++i)
          if (ImGui::MenuItem(primitives[i])) {
            std::string name = primitives[i];
            scene->GetSpawnManager().Spawn(name);
            ImGui::CloseCurrentPopup();
          }
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
