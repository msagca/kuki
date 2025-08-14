#include <system/rendering.hpp>
#include <application.hpp>
#include <editor.hpp>
#include <imgui.h>
#include <string>
#include <system/scripting.hpp>
#include <camera_controller.hpp>
#include <component/camera.hpp>
#include <component/script.hpp>
using namespace kuki;
void Editor::DisplayScene() {
  static const ImVec2 uv0(.0f, 1.0f);
  static const ImVec2 uv1(1.0f, .0f);
  static auto flying = false;
  ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse /*| ImGuiWindowFlags_MenuBar*/);
  if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !flying) {
    flying = true;
    ImGui::SetWindowFocus();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
  }
  if (flying && (ImGui::IsMouseReleased(ImGuiMouseButton_Right) || !ImGui::IsMouseDown(ImGuiMouseButton_Right))) {
    flying = false;
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
  }
  cameraController->ToggleRotation(flying);
  auto renderSystem = GetSystem<RenderingSystem>();
  if (renderSystem) {
    renderSystem->SetGizmoMask(context.gizmoMask); // NOTE: sets the render system specific gizmo flags
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
      DrawGizmos(drawWidth, drawHeight, context.gizmoMask); // NOTE: these gizmos are external to the render system, they draw on top of the rendered scene
    }
  }
  if (ImGui::BeginDragDropTarget()) {
    if (auto payload = ImGui::AcceptDragDropPayload("SPAWN_ASSET")) {
      std::string droppedAssetName = (const char*)payload->Data;
      Spawn(droppedAssetName);
    }
    ImGui::EndDragDropTarget();
  }
  if (context.displayFPS) {
    ImGui::SetCursorPos(ImVec2(ImGui::GetTextLineHeight(), ImGui::GetFrameHeight() + ImGui::GetTextLineHeight()));
    ImGui::Text("%d", GetFPS());
  }
  ImGui::End();
}
void Editor::ToggleFPS() {
  context.displayFPS = !context.displayFPS;
}
