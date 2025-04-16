#include <editor.hpp>
#include <event_dispatcher.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
using namespace kuki;
void Editor::DisplayHierarchy() {
  std::vector<unsigned int> entities;
  ForEachEntity([&](unsigned int id) {
    entities.push_back(id);
  });
  selection.UserData = reinterpret_cast<void*>(&entities);
  selection.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* self, int index) {
    auto* entityIDs = reinterpret_cast<std::vector<unsigned int>*>(self->UserData);
    if (index < 0 || index >= entityIDs->size())
      return static_cast<unsigned int>(-1);
    return (*entityIDs)[index];
  };
  ImGui::Begin("Hierarchy");
  ImGuiMultiSelectIO* ms_io = ImGui::BeginMultiSelect(ImGuiMultiSelectFlags_None, selection.Size, entities.size());
  selection.ApplyRequests(ms_io);
  selectedEntity = -1;
  for (const auto& id : entities) {
    auto selected = selection.Contains(id);
    ImGui::SetNextItemSelectionUserData(id);
    ImGui::Selectable(GetEntityName(id).c_str(), &selected);
    if (selected)
      selectedEntity = id;
  }
  ms_io = ImGui::EndMultiSelect();
  selection.ApplyRequests(ms_io);
  auto deleteSelection = GetKeyDown(GLFW_KEY_DELETE);
  if (deleteSelection && selection.Size > 0) {
    std::vector<unsigned int> entitiesToDelete;
    for (const auto& id : entities)
      if (selection.Contains(id))
        entitiesToDelete.push_back(id);
    for (const auto& id : entitiesToDelete)
      DeleteEntity(id);
  }
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
    selection.Clear();
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
  DisplayProperties();
}
void Editor::EntityCreatedCallback(const EntityCreatedEvent& event) {
  selection.Clear();
}
void Editor::EntityDeletedCallback(const EntityDeletedEvent& event) {
  selection.Clear();
}
