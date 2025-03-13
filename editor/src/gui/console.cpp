#include <editor.hpp>
#include <imgui_internal.h>
void Editor::DisplayConsole() {
  static const auto WINDOW_FLAGS = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav;
  static const auto WINDOW_PADDING = 10.0f;
  static const auto WARNING_COLOR = ImVec4(1.0f, .8f, .0f, 1.0f);
  static auto consoleVisible = false;
  if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
    consoleVisible = true;
    inputManager.DisableKeyCallbacks();
    ImGui::SetNextWindowFocus();
  }
  if (!consoleVisible)
    return;
  auto sceneWindow = ImGui::FindWindowByName("Scene");
  if (!sceneWindow || !sceneWindow->Active)
    return;
  auto contentPos = sceneWindow->Pos;
  contentPos.x += WINDOW_PADDING;
  contentPos.y += sceneWindow->TitleBarHeight + WINDOW_PADDING;
  auto contentAreaSize = sceneWindow->ContentRegionRect.GetSize();
  auto consolePos = contentPos;
  auto consoleHeight = ImGui::GetTextLineHeightWithSpacing();
  if (hasConsoleMessage)
    consoleHeight *= 3;
  auto consoleSize = ImVec2(contentAreaSize.x - WINDOW_PADDING, consoleHeight);
  ImGui::SetNextWindowPos(consolePos);
  ImGui::SetNextWindowSize(consoleSize);
  ImGui::SetNextWindowBgAlpha(0.5f);
  if (ImGui::Begin("Console", &consoleVisible, WINDOW_FLAGS)) {
    if (hasConsoleMessage)
      ImGui::TextColored(WARNING_COLOR, "%s", consoleMessage.c_str());
    ImGui::SetKeyboardFocusHere();
    static char commandBuffer[256] = "";
    if (ImGui::InputText("##Command", commandBuffer, IM_ARRAYSIZE(commandBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
      ProcessCommand(commandBuffer);
      if (!hasConsoleMessage) {
        commandBuffer[0] = '\0';
        consoleVisible = false;
      }
    }
  }
  ImGui::End();
  if (consoleVisible && ImGui::IsKeyReleased(ImGuiKey_Escape)) {
    consoleVisible = false;
    consoleMessage.clear();
    hasConsoleMessage = false;
  }
  if (!consoleVisible)
    inputManager.EnableKeyCallbacks();
}
