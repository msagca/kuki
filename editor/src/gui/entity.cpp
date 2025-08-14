#include <cstring>
#include <editor.hpp>
#include <imgui.h>
#include <string>
void Editor::DisplayEntity(unsigned int id) {
  static const auto INPUT_TEXT_FLAGS = ImGuiTreeNodeFlags(ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
  static auto renameMode = false;
  static auto entityBeingRenamed = -1;
  static char newName[256] = "";
  auto nodeFlags = ImGuiTreeNodeFlags(ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick);
  if (!EntityHasChildren(id))
    nodeFlags |= ImGuiTreeNodeFlags_Leaf;
  if (selection.Contains(id)) {
    nodeFlags |= ImGuiTreeNodeFlags_Selected;
    context.selectedEntity = id;
  }
  ImGui::SetNextItemSelectionUserData(id);
  if (renameMode && entityBeingRenamed == id) {
    // TODO: disable camera controls while renaming
    ImGui::AlignTextToFramePadding();
    ImGui::PushItemWidth(-1);
    ImGui::SetKeyboardFocusHere();
    auto renameConfirmed = ImGui::InputText("##Rename", newName, IM_ARRAYSIZE(newName), INPUT_TEXT_FLAGS);
    if (renameConfirmed) {
      std::string nameStr = newName;
      if (!nameStr.empty())
        RenameEntity(id, nameStr);
    }
    if (renameConfirmed || ImGui::IsItemDeactivated()) {
      renameMode = false;
      entityBeingRenamed = -1;
    }
    ImGui::PopItemWidth();
    return;
  }
  auto entityName = GetEntityName(id);
  const char* name = entityName.c_str();
  bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)id, nodeFlags, "%s", name);
  if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    strcpy(newName, name);
    renameMode = true;
    entityBeingRenamed = id;
  }
  if (nodeOpen) {
    ForEachChildEntity(id, [&](unsigned int childId) {
      DisplayEntity(childId);
    });
    ImGui::TreePop();
  }
}
