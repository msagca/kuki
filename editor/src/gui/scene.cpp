#include <application.hpp>
#include <editor.hpp>
#include <imgui.h>
#include <render_system.hpp>
#include <string>
using namespace kuki;
void Editor::DisplayScene() {
  static const ImVec2 uv0(.0f, 1.0f);
  static const ImVec2 uv1(1.0f, .0f);
  static auto gizmoMask = static_cast<unsigned int>(GizmoMask::Manipulator);
  static auto flying = false;
  ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar);
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("Gizmos")) {
      auto manipulatorEnabled = (gizmoMask & static_cast<unsigned int>(GizmoMask::Manipulator)) != 0;
      // TODO: manipulator gizmo is currently implemented by the editor, this won't have any impact on the render system
      if (ImGui::MenuItem("Manipulator", nullptr, manipulatorEnabled))
        gizmoMask ^= static_cast<unsigned int>(GizmoMask::Manipulator);
      auto viewFrustumEnabled = (gizmoMask & static_cast<unsigned int>(GizmoMask::ViewFrustum)) != 0;
      if (ImGui::MenuItem("View Frustum", nullptr, viewFrustumEnabled))
        gizmoMask ^= static_cast<unsigned int>(GizmoMask::ViewFrustum);
      auto frustumCullingEnabled = (gizmoMask & static_cast<unsigned int>(GizmoMask::FrustumCulling)) != 0;
      if (ImGui::MenuItem("Frustum Culling", nullptr, frustumCullingEnabled))
        gizmoMask ^= static_cast<unsigned int>(GizmoMask::FrustumCulling);
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }
  if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !flying) {
    flying = true;
    ImGui::SetWindowFocus();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
  }
  if (flying && (ImGui::IsMouseReleased(ImGuiMouseButton_Right) || !ImGui::IsMouseDown(ImGuiMouseButton_Right))) {
    flying = false;
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
  }
  cameraController.ToggleRotation(flying);
  cameraController.Update(deltaTime);
  auto renderSystem = GetSystem<RenderSystem>();
  if (renderSystem) {
    renderSystem->SetGizmoMask(gizmoMask); // NOTE: sets the render system specific gizmo flags
    auto texture = renderSystem->RenderSceneToTexture(&editorCamera);
    if (texture > 0) {
      auto& config = GetConfig();
      auto width = config.screenWidth;
      auto height = config.screenHeight;
      auto contentRegion = ImGui::GetContentRegionAvail();
      auto scaleFactor = std::max(contentRegion.x / width, contentRegion.y / height);
      auto drawWidth = width * scaleFactor;
      auto drawHeight = height * scaleFactor;
      ImGui::Image(texture, ImVec2(drawWidth, drawHeight), uv0, uv1);
      DrawGizmos(drawWidth, drawHeight, gizmoMask); // NOTE: these gizmos are external to the render system, they draw on top of the rendered scene
    }
  }
  if (ImGui::BeginDragDropTarget()) {
    if (auto payload = ImGui::AcceptDragDropPayload("SPAWN_ASSET")) {
      std::string droppedAssetName = (const char*)payload->Data;
      Spawn(droppedAssetName);
    }
    ImGui::EndDragDropTarget();
  }
  ImGui::End();
}
