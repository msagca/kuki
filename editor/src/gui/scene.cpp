#include <application.hpp>
#include <editor.hpp>
#include <imgui.h>
#include <render_system.hpp>
void Editor::DisplayScene() {
  static const ImVec2 uv0(.0f, 1.0f);
  static const ImVec2 uv1(1.0f, .0f);
  static auto isRotating = false;
  ImGui::Begin("Scene");
  if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !isRotating)
    isRotating = true;
  if (isRotating && (ImGui::IsMouseReleased(ImGuiMouseButton_Right) || !ImGui::IsMouseDown(ImGuiMouseButton_Right)))
    isRotating = false;
  cameraController.ToggleRotation(isRotating);
  cameraController.SetCamera(GetActiveCamera());
  cameraController.Update(deltaTime);
  auto renderSystem = GetSystem<RenderSystem>();
  auto contentRegion = ImGui::GetContentRegionAvail();
  if (renderSystem) {
    auto texture = renderSystem->RenderSceneToTexture(contentRegion.x, contentRegion.y);
    if (texture > 0) {
      ImGui::Image(texture, ImVec2(contentRegion.x, contentRegion.y), uv0, uv1);
      DrawGizmos();
    }
  }
  ImGui::End();
}
