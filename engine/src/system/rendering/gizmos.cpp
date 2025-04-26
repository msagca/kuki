#define GLM_ENABLE_EXPERIMENTAL
#include <glad/glad.h>
#include <application.hpp>
#include <component/mesh.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <system/rendering.hpp>
#include <utility/octree.hpp>
#include <component/shader.hpp>
#include <vector>
#include <component/material.hpp>
namespace kuki {
void RenderingSystem::DrawGizmos() {
  auto manipulatorEnabled = (gizmoMask & static_cast<unsigned int>(GizmoMask::Manipulator)) != 0;
  auto viewFrustumEnabled = (gizmoMask & static_cast<unsigned int>(GizmoMask::ViewFrustum)) != 0;
  auto frustumCullingEnabled = (gizmoMask & static_cast<unsigned int>(GizmoMask::FrustumCulling)) != 0;
  if (viewFrustumEnabled)
    DrawViewFrustum();
  if (frustumCullingEnabled)
    DrawFrustumCulling();
}
void RenderingSystem::DrawFrustumCulling() {
  auto assetId = app.GetAssetId("Cube");
  auto mesh = app.GetAssetComponent<Mesh>(assetId);
  if (!mesh)
    return;
  auto shader = static_cast<UnlitShader*>(shaders["Unlit"]);
  shader->Use();
  glm::vec4 color{};
  color.a = .2f;
  auto camera = app.GetActiveCamera();
  if (!camera)
    return;
  shader->SetUniform("view", targetCamera->view);
  shader->SetUniform("projection", targetCamera->projection);
  std::vector<glm::mat4> transforms;
  std::vector<UnlitFallbackData> materials;
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
    transforms.push_back(model);
    UnlitFallbackData material{};
    material.base = color;
    materials.push_back(material);
  });
  shader->SetInstanceData(mesh, transforms, materials, transformVBO, materialVBO);
  glBindVertexArray(mesh->vertexArray);
  if (mesh->indexCount > 0)
    glDrawElementsInstanced(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0, transforms.size());
  else
    glDrawArraysInstanced(GL_TRIANGLES, 0, mesh->vertexCount, transforms.size());
  glBindVertexArray(0);
}
void RenderingSystem::DrawViewFrustum() {
  auto assetId = app.GetAssetId("Cube");
  auto mesh = app.GetAssetComponent<Mesh>(assetId);
  if (!mesh)
    return;
  auto shader = static_cast<UnlitShader*>(shaders["Unlit"]);
  shader->Use();
  auto sceneCamera = app.GetActiveCamera();
  if (!sceneCamera)
    return;
  auto model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f)); // scale up local coordinates (-.5f, .5f) to NDC (-1.0f, 1.0f)
  model = glm::inverse(sceneCamera->projection * sceneCamera->view) * model;
  shader->SetUniform("view", targetCamera->view);
  shader->SetUniform("projection", targetCamera->projection);
  std::vector<glm::mat4> transforms{model};
  UnlitFallbackData material{};
  material.base = glm::vec4(.5f, .5f, .0f, .2f);
  std::vector<UnlitFallbackData> materials{material};
  shader->SetInstanceData(mesh, transforms, materials, transformVBO, materialVBO);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(mesh->vertexArray);
  if (mesh->indexCount > 0)
    glDrawElementsInstanced(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0, transforms.size());
  else
    glDrawArraysInstanced(GL_TRIANGLES, 0, mesh->vertexCount, transforms.size());
  glBindVertexArray(0);
  glEnable(GL_CULL_FACE);
}
unsigned int RenderingSystem::GetGizmoMask() const {
  return gizmoMask;
}
void RenderingSystem::SetGizmoMask(unsigned int mask) {
  gizmoMask = mask;
}
} // namespace kuki
