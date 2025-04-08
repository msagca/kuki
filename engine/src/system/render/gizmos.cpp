#define GLM_ENABLE_EXPERIMENTAL
#include <application.hpp>
#include <render_system.hpp>
#include <component/mesh.hpp>
#include <utility/octree.hpp>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
void RenderSystem::DrawGizmos() {
  auto manipulatorEnabled = (gizmoMask & static_cast<unsigned int>(GizmoMask::Manipulator)) != 0;
  auto viewFrustumEnabled = (gizmoMask & static_cast<unsigned int>(GizmoMask::ViewFrustum)) != 0;
  auto frustumCullingEnabled = (gizmoMask & static_cast<unsigned int>(GizmoMask::FrustumCulling)) != 0;
  if (viewFrustumEnabled)
    DrawViewFrustum();
  if (frustumCullingEnabled)
    DrawFrustumCulling();
}
void RenderSystem::DrawFrustumCulling() {
  auto assetId = app.GetAssetId("Cube");
  auto mesh = app.GetAssetComponent<Mesh>(assetId);
  if (!mesh)
    return;
  auto shader = shaders["Unlit"];
  shader->Use();
  glm::vec4 color{};
  color.a = .2f;
  auto camera = app.GetActiveCamera();
  if (!camera)
    return;
  app.ForEachOctreeLeafNode([&](Octree<unsigned int>* octree, Octant octant) {
    auto depth = octree->GetDepth();
    auto maxDepth = octree->GetMaxDepth();
    auto center = octree->GetCenter();
    auto extents = octree->GetExtent();
    auto overlaps = camera->OverlapsFrustum(octree->GetBounds());
    auto model = glm::mat4(1.0f);
    model = glm::translate(model, center);
    model = glm::scale(model, extents * 2.0f);
    auto ratio = static_cast<unsigned int>(octant) / 16.0f;
    color.r = overlaps ? .0f : .5f + ratio;
    color.g = overlaps ? .5f + ratio : .0f;
    color.b = maxDepth > 0 ? static_cast<float>(depth) / maxDepth : 1.0f;
    shader->SetUniform("useBaseFallback", true);
    shader->SetUniform("fallback.base", color);
    shader->SetUniform("model", model);
    shader->SetUniform("view", targetCamera->view);
    shader->SetUniform("projection", targetCamera->projection);
    glBindVertexArray(mesh->vertexArray);
    if (mesh->indexCount > 0)
      glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
    else
      glDrawArrays(GL_TRIANGLES, 0, mesh->vertexCount);
  });
}
void RenderSystem::DrawViewFrustum() {
  auto assetId = app.GetAssetId("Cube");
  auto mesh = app.GetAssetComponent<Mesh>(assetId);
  if (!mesh)
    return;
  auto shader = shaders["Unlit"];
  shader->Use();
  auto sceneCamera = app.GetActiveCamera();
  if (!sceneCamera)
    return;
  auto model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f)); // scale up local coordinates (-.5f, .5f) to NDC (-1.0f, 1.0f)
  model = glm::inverse(sceneCamera->projection * sceneCamera->view) * model;
  shader->SetUniform("useBaseFallback", true);
  shader->SetUniform("fallback.base", glm::vec4(.5f, .5f, .0f, .2f));
  shader->SetUniform("model", model);
  shader->SetUniform("view", targetCamera->view);
  shader->SetUniform("projection", targetCamera->projection);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(mesh->vertexArray);
  if (mesh->indexCount > 0)
    glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
  else
    glDrawArrays(GL_TRIANGLES, 0, mesh->vertexCount);
  glEnable(GL_CULL_FACE);
}
unsigned int RenderSystem::GetGizmoMask() const {
  return gizmoMask;
}
void RenderSystem::SetGizmoMask(unsigned int mask) {
  gizmoMask = mask;
}
