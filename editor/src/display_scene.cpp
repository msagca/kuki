#include <algorithm>
#include <application.hpp>
#include <camera_controller.hpp>
#include <editor.hpp>
#include <glfw_constants.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <rendering_system.hpp>
#include <string>
using namespace kuki;
void Editor::DisplayScene() {
  static const ImVec2 uv0(.0f, 1.0f);
  static const ImVec2 uv1(1.0f, .0f);
  ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  const auto fPressed = ImGui::IsKeyPressed(ImGuiKey_F);
  const auto focused = ImGui::IsWindowFocused();
  static auto wasFocused = false;
  if (focused != wasFocused) {
    if (!wasFocused)
      EnableKeys();
    else
      DisableKeys();
    wasFocused = focused;
  }
  if (focused && fPressed && !showConsole)
    showFPS = !showFPS;
  auto renderSystem = GetSystem<RenderingSystem>();
  if (renderSystem) {
    renderSystem->SetGizmoMask(context.gizmoMask);
    auto texture = renderSystem->RenderSceneToTexture(cameraController->GetCamera());
    if (texture > 0) {
      auto& config = GetConfig();
      auto width = config.screenWidth;
      auto height = config.screenHeight;
      auto contentRegion = ImGui::GetContentRegionAvail();
      auto scaleFactor = std::max(contentRegion.x / width, contentRegion.y / height);
      auto drawWidth = width * scaleFactor;
      auto drawHeight = height * scaleFactor;
      ImGui::Image(texture, ImVec2(drawWidth, drawHeight), uv0, uv1);
      auto manipulatorEnabled = (context.gizmoMask & static_cast<size_t>(GizmoMask::Manipulator)) != 0;
      if (manipulatorEnabled)
        DrawManipulator(drawWidth, drawHeight);
    }
  }
  if (ImGui::BeginDragDropTarget()) {
    if (auto payload = ImGui::AcceptDragDropPayload("SPAWN_ASSET")) {
      std::string droppedAssetName = (const char*)payload->Data;
      Instantiate(droppedAssetName);
      ImGui::SetWindowFocus();
    }
    ImGui::EndDragDropTarget();
  }
  if (showFPS) {
    ImGui::SetCursorPos(ImVec2(ImGui::GetTextLineHeight(), ImGui::GetFrameHeight() + ImGui::GetTextLineHeight()));
    ImGui::Text("%zu", GetFPS());
  }
  static char commandBuffer[256] = "";
  static bool displayMessage = false;
  static std::string commandMessage;
  if (showConsole && escapePressed) {
    showConsole = false;
    commandBuffer[0] = '\0';
    commandMessage.clear();
    displayMessage = false;
  } else if (focused && !showConsole && enterPressed)
    showConsole = true;
  if (showConsole) {
    ImVec2 consoleSize(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeightWithSpacing() * (displayMessage ? 3 : 1));
    ImGui::BeginChild("ConsoleChild", consoleSize, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav);
    if (displayMessage)
      ImGui::TextColored(ImVec4(1.0f, .8f, .0f, 1.0f), "%s", commandMessage.c_str());
    ImGui::SetKeyboardFocusHere();
    if (ImGui::InputText("##Command", commandBuffer, IM_ARRAYSIZE(commandBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
      auto result = DispatchCommand(commandBuffer, commandMessage);
      commandBuffer[0] = '\0';
      displayMessage = result != 0;
      showConsole = displayMessage;
    }
    ImGui::EndChild();
  }
  ImGui::End();
}
