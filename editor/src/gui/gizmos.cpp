#include <camera_controller.hpp>
#include <component/camera.hpp>
#include <component/transform.hpp>
#include <editor.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ImGuizmo.h>
void Editor::DisplayGizmos() {
  auto activeScene = GetActiveScene();
  if (!activeScene)
    return;
  auto camera = cameraController.GetCameraPtr();
  if (!camera)
    return;
  ImGuizmo::SetDrawlist();
  auto windowPos = ImGui::GetWindowPos();
  auto windowSize = ImGui::GetWindowSize();
  ImGuizmo::SetRect(windowPos.x, windowPos.y, windowSize.x, windowSize.y);
  ImGuizmo::SetOrthographic(false);
  static const auto IDENTITY_MATRIX = glm::mat4(1.0f);
  ImGuizmo::DrawGrid(glm::value_ptr(camera->view), glm::value_ptr(camera->projection), glm::value_ptr(IDENTITY_MATRIX), camera->farPlane);
  auto& entityManager = activeScene->GetEntityManager();
  auto transform = entityManager.GetComponent<Transform>(selectedEntity);
  if (!transform)
    return;
  glm::mat4 matrix;
  auto rotation = glm::degrees(glm::eulerAngles(transform->rotation));
  ImGuizmo::RecomposeMatrixFromComponents(glm::value_ptr(transform->position), glm::value_ptr(rotation), glm::value_ptr(transform->scale), glm::value_ptr(matrix));
  ImGuizmo::Manipulate(glm::value_ptr(camera->view), glm::value_ptr(camera->projection), ImGuizmo::OPERATION::UNIVERSAL, ImGuizmo::MODE::WORLD, glm::value_ptr(matrix));
  ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(matrix), glm::value_ptr(transform->position), glm::value_ptr(rotation), glm::value_ptr(transform->scale));
  transform->rotation = glm::quat(glm::radians(rotation));
}
