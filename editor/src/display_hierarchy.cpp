#include <editor.hpp>
#include <glfw_constants.hpp>
#include <id.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
using namespace kuki;
void Editor::DisplayHierarchy() {
  auto& io = ImGui::GetIO();
  ctrlHeld = io.KeyCtrl;
  shiftHeld = io.KeyShift;
  backspacePressed = ImGui::IsKeyPressed(ImGuiKey_Backspace);
  deletePressed = ImGui::IsKeyPressed(ImGuiKey_Delete);
  enterPressed = ImGui::IsKeyPressed(ImGuiKey_Enter);
  escapePressed = ImGui::IsKeyPressed(ImGuiKey_Escape);
  spacePressed = ImGui::IsKeyPressed(ImGuiKey_Space);
  ImGui::Begin("Hierarchy");
  const auto focused = ImGui::IsWindowFocused();
  const auto clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  const auto windowHovered = ImGui::IsWindowHovered();
  const auto itemsHovered = ImGui::IsAnyItemHovered();
  auto clearSelection = backspacePressed || deletePressed || escapePressed || (clicked && windowHovered && !itemsHovered);
  if (deletePressed)
    for (const auto& entityId : selection)
      DeleteEntity(entityId);
  if (focused && clearSelection) {
    selection.clear();
    context.selectedEntity = ID::Invalid();
    context.assetMask = -1;
  }
  displayedEntities.clear();
  ForEachRootEntity([this](ID id) {
    DisplayEntity(id);
  });
  if (ImGui::BeginPopupContextWindow("CreateMenu", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
    if (ImGui::BeginMenu("Create")) {
      if (ImGui::MenuItem("Empty")) {
        std::string name = "Entity";
        CreateEntity(name);
        spdlog::info("Created a new entity.");
        ImGui::CloseCurrentPopup();
      }
      if (ImGui::BeginMenu("Asset")) {
        ForEachRootAsset([this](ID id) {
          auto name = GetAssetName(id);
          if (ImGui::MenuItem(name.c_str())) {
            Instantiate(name);
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
