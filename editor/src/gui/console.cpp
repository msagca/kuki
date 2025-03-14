#include <editor.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <string>
void Editor::DisplayConsole() {
  static const auto WINDOW_FLAGS = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav;
  static const auto WINDOW_PADDING = 10.0f;
  static const auto WARNING_COLOR = ImVec4(1.0f, .8f, .0f, 1.0f);
  static char commandBuffer[256] = "";
  static auto consoleVisible = false;
  static bool displayMessage = false;
  static std::string commandMessage;
  if (!consoleVisible && GetKey(GLFW_KEY_SPACE))
    consoleVisible = true;
  if (consoleVisible && GetKey(GLFW_KEY_ESCAPE))
    consoleVisible = false;
  if (consoleVisible)
    DisableKeys<int>(GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_A, GLFW_KEY_D, GLFW_KEY_V);
  else {
    EnableKeys<int>(GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_A, GLFW_KEY_D, GLFW_KEY_V);
    commandBuffer[0] = '\0';
    commandMessage.clear();
    displayMessage = false;
    return;
  }
  ImGui::SetNextWindowFocus();
  auto sceneWindow = ImGui::FindWindowByName("Scene");
  if (!sceneWindow || !sceneWindow->Active)
    return;
  auto contentPos = sceneWindow->Pos;
  contentPos.x += WINDOW_PADDING;
  contentPos.y += sceneWindow->TitleBarHeight + WINDOW_PADDING;
  auto contentAreaSize = sceneWindow->ContentRegionRect.GetSize();
  auto consolePos = contentPos;
  auto consoleHeight = ImGui::GetTextLineHeightWithSpacing();
  if (displayMessage)
    consoleHeight *= 3;
  auto consoleSize = ImVec2(contentAreaSize.x - WINDOW_PADDING, consoleHeight);
  ImGui::SetNextWindowPos(consolePos);
  ImGui::SetNextWindowSize(consoleSize);
  ImGui::SetNextWindowBgAlpha(0.5f);
  if (ImGui::Begin("Console", &consoleVisible, WINDOW_FLAGS)) {
    if (displayMessage)
      ImGui::TextColored(WARNING_COLOR, "%s", commandMessage.c_str());
    ImGui::SetKeyboardFocusHere();
    if (ImGui::InputText("##Command", commandBuffer, IM_ARRAYSIZE(commandBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
      auto result = DispatchCommand(commandBuffer, commandMessage);
      commandBuffer[0] = '\0';
      displayMessage = result != 0;
      consoleVisible = displayMessage;
    }
  }
  ImGui::End();
}
