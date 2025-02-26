#include <application.hpp>
#include <camera_controller.hpp>
#include <editor.hpp>
#include <imgui.h>
#include <render_system.hpp>
void Editor::DisplayScene() {
  static const ImVec2 uv0(.0f, 1.0f);
  static const ImVec2 uv1(1.0f, .0f);
  ImGui::Begin("Scene");
  auto contentRegion = ImGui::GetContentRegionAvail();
  auto scene = GetActiveScene();
  auto renderSystem = GetSystem<RenderSystem>();
  if (renderSystem) {
    auto texture = renderSystem->RenderSceneToTexture(cameraController.GetCamera(), contentRegion.x, contentRegion.y, selectedEntity);
    if (texture >= 0)
      ImGui::Image(texture, ImVec2(contentRegion.x, contentRegion.y), uv0, uv1);
  }
  ImGui::End();
}
