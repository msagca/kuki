#define GLM_ENABLE_EXPERIMENTAL
#include <system/rendering.hpp>
#include <application.hpp>
#include <component/component.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/shader.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <utility/octree.hpp>
#include <vector>
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
  auto camera = app.GetActiveCamera();
  if (!camera)
    return;
  auto assetId = app.GetAssetId("Cube");
  auto mesh = app.GetAssetComponent<Mesh>(assetId);
  if (!mesh)
    return;
  glm::vec4 color{};
  color.a = .2f;
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
  auto shader = static_cast<UnlitShader*>(GetShader(MaterialType::Unlit));
  shader->Use();
  shader->SetCamera(targetCamera);
  shader->SetMaterialFallback(mesh, materials, materialVBO);
  shader->SetTransform(mesh, transforms, transformVBO);
  shader->DrawInstanced(mesh, transforms.size());
}
void RenderingSystem::DrawViewFrustum() {
  static Material material;
  auto camera = app.GetActiveCamera();
  if (!camera)
    return;
  auto assetId = app.GetAssetId("Cube");
  auto mesh = app.GetAssetComponent<Mesh>(assetId);
  if (!mesh)
    return;
  material.material = UnlitMaterial();
  auto unlitMaterial = std::get<UnlitMaterial>(material.material);
  unlitMaterial.fallback.base = glm::vec4(.5f, .5f, .0f, .2f);
  auto model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f)); // scale up local coordinates (-.5f, .5f) to NDC (-1.0f, 1.0f)
  model = glm::inverse(camera->projection * camera->view) * model;
  auto shader = static_cast<UnlitShader*>(GetShader(MaterialType::Unlit));
  shader->Use();
  shader->SetCamera(targetCamera);
  shader->SetMaterial(&material);
  shader->SetMaterialFallback(mesh, unlitMaterial.fallback, materialVBO);
  shader->SetTransform(mesh, model, transformVBO);
  glDisable(GL_CULL_FACE);
  shader->DrawInstanced(mesh, 1);
  glEnable(GL_CULL_FACE);
}
unsigned int RenderingSystem::GetGizmoMask() const {
  return gizmoMask;
}
void RenderingSystem::SetGizmoMask(unsigned int mask) {
  gizmoMask = mask;
}
} // namespace kuki
