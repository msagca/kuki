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
void RenderingSystem::DrawGizmos(const Camera* camera, const Camera* observer) {
  if (!camera || camera == observer)
    return;
  auto manipulatorEnabled = (gizmoMask & static_cast<unsigned int>(GizmoMask::Manipulator)) != 0;
  auto viewFrustumEnabled = (gizmoMask & static_cast<unsigned int>(GizmoMask::ViewFrustum)) != 0;
  auto frustumCullingEnabled = (gizmoMask & static_cast<unsigned int>(GizmoMask::FrustumCulling)) != 0;
  if (viewFrustumEnabled)
    DrawViewFrustum(camera, observer);
  if (frustumCullingEnabled)
    DrawFrustumCulling(camera, observer);
}
void RenderingSystem::DrawFrustumCulling(const Camera* camera, const Camera* observer) {
  auto assetId = app.GetAssetId("Cube");
  auto mesh = app.GetAssetComponent<Mesh>(assetId);
  if (!mesh)
    return;
  glm::vec4 color{};
  color.a = .2f;
  std::vector<glm::mat4> transforms;
  std::vector<UnlitFallbackData> materials;
  app.ForEachOctreeLeafNode([&](OctreeNode* node, Octant octant) {
    auto depth = node->depth;
    auto maxDepth = node->maxDepth;
    auto center = node->center;
    auto extent = node->extent;
    auto intersects = camera->IntersectsFrustum(node->bounds);
    auto model = glm::mat4(1.0f);
    model = glm::translate(model, center);
    model = glm::scale(model, extent * 2.0f);
    auto ratio = static_cast<unsigned int>(octant) / 16.0f;
    color.r = intersects ? .0f : .5f + ratio;
    color.g = intersects ? .5f + ratio : .0f;
    color.b = maxDepth > 0 ? static_cast<float>(depth) / maxDepth : 1.0f;
    transforms.push_back(model);
    UnlitFallbackData material{};
    material.base = color;
    materials.push_back(material);
  });
  auto shader = static_cast<UnlitShader*>(GetShader(MaterialType::Unlit));
  shader->Use();
  shader->SetCamera(observer);
  shader->SetMaterialFallback(mesh, materials, materialVBO);
  shader->SetTransform(mesh, transforms, transformVBO);
  shader->DrawInstanced(mesh, transforms.size());
}
void RenderingSystem::DrawViewFrustum(const Camera* camera, const Camera* observer) {
  static Material material;
  auto frameId = app.GetAssetId("Frame");
  auto frameMesh = app.GetAssetComponent<Mesh>(frameId);
  if (!frameMesh)
    return;
  material.material = UnlitMaterial();
  auto unlitMaterial = std::get<UnlitMaterial>(material.material);
  unlitMaterial.fallback.base = glm::vec4(.5f, .5f, .0f, .2f);
  auto shader = static_cast<UnlitShader*>(GetShader(MaterialType::Unlit));
  shader->Use();
  shader->SetCamera(observer);
  shader->SetMaterial(&material);
  shader->SetMaterialFallback(frameMesh, unlitMaterial.fallback, materialVBO);
  glDisable(GL_CULL_FACE);
  // near plane (z = -1 in NDC)
  auto nearModel = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 2.0f, 1.0f));
  nearModel = glm::translate(nearModel, glm::vec3(0.0f, 0.0f, -1.0f));
  nearModel = glm::inverse(camera->projection * camera->view) * nearModel;
  shader->SetTransform(frameMesh, nearModel, transformVBO);
  shader->DrawInstanced(frameMesh, 1);
  // far plane (z = 1 in NDC)
  auto farModel = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 2.0f, 1.0f));
  farModel = glm::translate(farModel, glm::vec3(0.0f, 0.0f, 1.0f));
  farModel = glm::inverse(camera->projection * camera->view) * farModel;
  shader->SetTransform(frameMesh, farModel, transformVBO);
  shader->DrawInstanced(frameMesh, 1);
  glEnable(GL_CULL_FACE);
}
unsigned int RenderingSystem::GetGizmoMask() const {
  return gizmoMask;
}
void RenderingSystem::SetGizmoMask(unsigned int mask) {
  gizmoMask = mask;
}
} // namespace kuki
