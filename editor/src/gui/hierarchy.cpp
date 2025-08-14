#include <editor.hpp>
#include <event_dispatcher.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
using namespace kuki;
void Editor::DisplayHierarchy() {
  ImGui::Begin("Hierarchy");
  ImGuiMultiSelectIO* ms_io = ImGui::BeginMultiSelect(ImGuiMultiSelectFlags_None, selection.Size, GetEntityCount());
  selection.ApplyRequests(ms_io);
  context.selectedEntity = -1;
  RemoveDeletedEntitiesFromSelection();
  ForEachRootEntity([this](unsigned int id) {
    DisplayEntity(id);
  });
  ms_io = ImGui::EndMultiSelect();
  selection.ApplyRequests(ms_io);
  auto deleteSelection = GetKeyDown(GLFW_KEY_DELETE);
  if (deleteSelection && selection.Size > 0) {
    std::vector<unsigned int> toDelete;
    void* it = nullptr;
    ImGuiID id;
    while (selection.GetNextSelectedItem(&it, &id))
      toDelete.push_back(static_cast<unsigned int>(id));
    for (const auto& entityId : toDelete) {
      selection.SetItemSelected(entityId, false);
      DeleteEntity(entityId);
    }
  }
  if (GetButtonDown(GLFW_MOUSE_BUTTON_LEFT) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) {
    selection.Clear();
    context.assetMask = -1;
  }
  if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && GetButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
    ImGui::OpenPopup("CreateMenu");
  if (ImGui::BeginPopup("CreateMenu")) {
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
  DisplayComponents();
}
void Editor::RemoveDeletedEntitiesFromSelection() {
  std::vector<unsigned int> toRemove;
  void* it = nullptr;
  ImGuiID id;
  while (selection.GetNextSelectedItem(&it, &id)) {
    auto entityId = static_cast<unsigned int>(id);
    if (!IsEntity(entityId))
      toRemove.push_back(entityId);
  }
  for (auto entityId : toRemove)
    selection.SetItemSelected(entityId, false);
}
std::vector<unsigned int> Editor::GetSelectedEntityIds() {
  std::vector<unsigned int> selectedIds;
  void* it = nullptr;
  ImGuiID id;
  while (selection.GetNextSelectedItem(&it, &id))
    selectedIds.push_back(static_cast<unsigned int>(id));
  return selectedIds;
}
