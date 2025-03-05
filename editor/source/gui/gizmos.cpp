#include <camera_controller.hpp>
#include <component/camera.hpp>
#include <component/transform.hpp>
#include <editor.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
void Editor::DisplayGizmos(Transform* transform) {
  if (!transform)
    return;
  auto windowPos = ImGui::GetWindowPos();
  ImGuizmo::SetDrawlist();
  auto displaySize = ImGui::GetIO().DisplaySize;
  ImGuizmo::SetRect(.0f, .0f, windowPos.x, windowPos.y);
  ImGuizmo::SetOrthographic(false);
  const auto& camera = cameraController.GetCamera();
  static const auto IDENTITY_MATRIX = glm::mat4(1.0f);
  ImGuizmo::BeginFrame();
  ImGuizmo::DrawGrid(glm::value_ptr(camera.view), glm::value_ptr(camera.projection), glm::value_ptr(IDENTITY_MATRIX), camera.farPlane);
  glm::mat4 matrix;
  auto rotation = glm::degrees(glm::eulerAngles(transform->rotation));
  ImGuizmo::RecomposeMatrixFromComponents(glm::value_ptr(transform->position), glm::value_ptr(rotation), glm::value_ptr(transform->scale), glm::value_ptr(matrix));
  ImGuizmo::Manipulate(glm::value_ptr(camera.view), glm::value_ptr(camera.projection), ImGuizmo::OPERATION::UNIVERSAL, ImGuizmo::MODE::WORLD, glm::value_ptr(matrix));
  ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(matrix), glm::value_ptr(transform->position), glm::value_ptr(rotation), glm::value_ptr(transform->scale));
  transform->rotation = glm::quat(glm::radians(rotation));
}
