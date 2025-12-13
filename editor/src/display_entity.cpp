#include <editor.hpp>
#include <glfw_constants.hpp>
#include <id.hpp>
#include <imgui.h>
#include <string>
void Editor::DisplayEntity(ID id) {
  static constexpr auto INPUT_TEXT_FLAGS = ImGuiTreeNodeFlags(ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
  static constexpr auto NAME_LENGTH = 256;
  static auto renameMode = false;
  static auto entityBeingRenamed = ID::Invalid();
  static char newName[NAME_LENGTH] = "";
  auto nodeFlags = ImGuiTreeNodeFlags(ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_NavLeftJumpsToParent);
  if (!EntityHasChildren(id))
    nodeFlags |= ImGuiTreeNodeFlags_Leaf;
  if (selection.find(id) != selection.end())
    nodeFlags |= ImGuiTreeNodeFlags_Selected;
  if (renameMode && entityBeingRenamed == id) {
    ImGui::AlignTextToFramePadding();
    ImGui::PushItemWidth(-1);
    ImGui::SetKeyboardFocusHere();
    auto renamed = ImGui::InputText("##Rename", newName, NAME_LENGTH, INPUT_TEXT_FLAGS);
    if (renamed) {
      std::string nameStr = newName;
      if (!nameStr.empty())
        RenameEntity(id, nameStr);
    }
    if (renamed || ImGui::IsItemDeactivated()) {
      renameMode = false;
      entityBeingRenamed = ID::Invalid();
    }
    ImGui::PopItemWidth();
    return;
  }
  auto entityName = GetEntityName(id);
  auto name = entityName.c_str();
  auto nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)id, nodeFlags, "%s", name);
  displayedEntities.push_back(id);
  auto const hovered = ImGui::IsItemHovered();
  auto const focused = ImGui::IsItemFocused();
  auto const clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
  auto const doubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
  if (clicked) {
    if (shiftHeld) {
      auto it1 = std::find(displayedEntities.begin(), displayedEntities.end(), context.selectedEntity);
      auto it2 = std::find(displayedEntities.begin(), displayedEntities.end(), id);
      if (it1 != displayedEntities.end() && it2 != displayedEntities.end()) {
        if (it1 > it2)
          // if it1 is below it2 in the list, swap them
          std::swap(it1, it2);
        if (!ctrlHeld)
          // if CTRL is pressed, append range to selection; otherwise, replace selection with range
          selection.clear();
        for (auto it = it1; it <= it2; ++it)
          // add items between the two entities (inclusive) to selection
          selection.insert(*it);
      }
    } else if (ctrlHeld) {
      if (selection.contains(id)) {
        selection.erase(id);
        context.selectedEntity = ID::Invalid();
      } else
        selection.insert(id);
    } else {
      selection.clear();
      selection.insert(id);
    }
    context.selectedEntity = id;
  } else if (focused && (shiftHeld || ctrlHeld)) {
    selection.insert(id);
    context.selectedEntity = id;
  } else if (focused && (enterPressed || spacePressed)) {
    selection.clear();
    context.selectedEntity = id;
  }
  if (hovered && doubleClicked) {
    strncpy(newName, name, NAME_LENGTH);
    newName[NAME_LENGTH - 1] = '\0';
    renameMode = true;
    entityBeingRenamed = id;
  }
  if (nodeOpen) {
    ForEachChildEntity(id, [&](ID childId) {
      DisplayEntity(childId);
    });
    ImGui::TreePop();
  }
}
