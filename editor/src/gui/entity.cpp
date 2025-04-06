#include <editor.hpp>
#include <string>
#include <imgui.h>
void Editor::DisplayEntity(unsigned int id) {
  static auto renameMode = false;
  static auto renamedEntity = -1;
  static char newName[256] = "";
  ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
  if (selectedEntities.find(id) != selectedEntities.end() || id >= lastSelection && id <= currentSelection)
    nodeFlags |= ImGuiTreeNodeFlags_Selected;
  if (!EntityHasChildren(id))
    nodeFlags |= ImGuiTreeNodeFlags_Leaf;
  auto renaming = renamedEntity == id && renameMode;
  auto nodeOpen = false;
  if (renaming) {
    // TODO: disable camera controls while renaming
    ImGui::AlignTextToFramePadding();
    ImGui::PushItemWidth(-1);
    ImGui::SetKeyboardFocusHere();
    auto renameConfirmed = ImGui::InputText("##Rename", newName, IM_ARRAYSIZE(newName), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
    if (renameConfirmed || ImGui::IsItemDeactivated()) {
      if (renameConfirmed || (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit())) {
        std::string nameStr = newName;
        if (!nameStr.empty())
          RenameEntity(id, nameStr);
      }
      renameMode = false;
      renamedEntity = -1;
    }
    ImGui::PopItemWidth();
  } else {
    auto label = GetEntityName(id);
    nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)id, nodeFlags, "%s", label.c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
      if (!addRangeToSelection) {
        lastSelection = id;
        if (!addToSelection)
          clearSelection = true;
      } else
        lastSelection = currentSelection;
      currentSelection = id;
    }
    // TODO: implement double click support in input manager
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      strcpy(newName, GetEntityName(id).c_str());
      renameMode = true;
      renamedEntity = id;
    }
  }
  if (nodeOpen) {
    ForEachChildEntity(id, [&](unsigned int childId) {
      DisplayEntity(childId);
    });
    if (!renaming)
      ImGui::TreePop();
  }
};
