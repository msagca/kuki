#include <application.hpp>
#include <editor.hpp>
#include <imgui.h>
#include <render_system.hpp>
void Editor::DisplayScene() {
  static const ImVec2 uv0(.0f, 1.0f);
  static const ImVec2 uv1(1.0f, .0f);
  static auto isRotating = false;
  ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !isRotating)
    isRotating = true;
  if (isRotating && (ImGui::IsMouseReleased(ImGuiMouseButton_Right) || !ImGui::IsMouseDown(ImGuiMouseButton_Right)))
    isRotating = false;
  cameraController.ToggleRotation(isRotating);
  cameraController.SetCamera(GetActiveCamera());
  cameraController.Update(deltaTime);
  auto renderSystem = GetSystem<RenderSystem>();
  if (renderSystem) {
    auto texture = renderSystem->RenderSceneToTexture();
    if (texture > 0) {
      auto config = GetConfig();
      auto width = config.screenWidth;
      auto height = config.screenHeight;
      auto contentRegion = ImGui::GetContentRegionAvail();
      auto scaleFactor = std::max(contentRegion.x / width, contentRegion.y / height);
      auto drawWidth = width * scaleFactor;
      auto drawHeight = height * scaleFactor;
      ImGui::Image(texture, ImVec2(drawWidth, drawHeight), uv0, uv1);
      DrawGizmos(drawWidth, drawHeight);
    }
  }
  ImGui::End();
}
