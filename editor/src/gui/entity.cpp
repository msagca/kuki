#include <editor.hpp>
#include <string>
void Editor::DisplayEntity(unsigned int id) {
  static auto renameMode = false;
  static auto renamedEntity = -1;
  static char newName[256] = "";
  ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
  if (id == selectedEntity)
    nodeFlags |= ImGuiTreeNodeFlags_Selected;
  if (!EntityHasChildren(id))
    nodeFlags |= ImGuiTreeNodeFlags_Leaf;
  auto renaming = (renamedEntity == id && renameMode);
  auto nodeOpen = false;
  if (renaming) {
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
      selectedEntity = id;
      clickedEntity = id;
    }
    auto popupName = "EntityContext#" + std::to_string(id);
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      selectedEntity = id;
      ImGui::OpenPopup(popupName.c_str());
    }
    if (ImGui::BeginPopup(popupName.c_str())) {
      if (ImGui::MenuItem("Remove")) {
        entityToDelete = id;
        if (selectedEntity == id)
          selectedEntity = -1;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
    if (GetKey(GLFW_KEY_DELETE) && selectedEntity == id) {
      entityToDelete = id;
      selectedEntity = -1;
      clickedEntity = -1;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      strcpy(newName, GetEntityName(id).c_str());
      renameMode = true;
      renamedEntity = id;
    }
  }
  if (nodeOpen) {
    ForEachChildOfEntity(id, [&](unsigned int childID) {
      DisplayEntity(childID);
    });
    if (!renaming)
      ImGui::TreePop();
  }
};
