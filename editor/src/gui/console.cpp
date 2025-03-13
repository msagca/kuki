#include <editor.hpp>
void Editor::DisplayConsole() {
  static bool consoleVisible = false;
  if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
    consoleVisible = true;
    inputManager.DisableKeyCallbacks();
  }
  if (!consoleVisible)
    return;
  ImGui::Begin("Console", &consoleVisible, ImGuiWindowFlags_NoCollapse);
  static char commandBuffer[256] = "";
  if (ImGui::InputText("Command", commandBuffer, IM_ARRAYSIZE(commandBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
    ProcessCommand(commandBuffer);
    commandBuffer[0] = '\0';
    consoleVisible = false;
    inputManager.EnableKeyCallbacks();
  }
  if (ImGui::IsKeyReleased(ImGuiKey_Escape)) {
    consoleVisible = false;
    inputManager.EnableKeyCallbacks();
  }
  ImGui::End();
}
