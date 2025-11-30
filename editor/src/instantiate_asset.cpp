#include <bone_data.hpp>
#include <cmath>
#include <editor.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <material.hpp>
#include <mesh.hpp>
#include <mesh_filter.hpp>
#include <mesh_renderer.hpp>
#include <random>
#include <string>
#include <transform.hpp>
using namespace kuki;
ID Editor::Instantiate(std::string& name, const ID parentId, glm::vec3 position, glm::quat rotation, glm::vec3 scale) {
  const auto assetId = GetAssetId(name);
  if (!assetId.IsValid())
    return ID::Invalid();
  const auto entityId = CreateEntity(name);
  const auto [transform, mesh, material, boneData] = GetAssetComponents<Transform, Mesh, Material, BoneData>(assetId);
  if (transform) {
    auto entityTransform = AddEntityComponent<Transform>(entityId);
    if (!parentId.IsValid()) {
      // NOTE: child entities must retain their relative transform to parent, assign given values only if no parent
      entityTransform->position = position;
      entityTransform->rotation = rotation;
      entityTransform->scale = scale;
    } else {
      *entityTransform = *transform;
      AddChildEntity(parentId, entityId);
    }
  }
  if (mesh) {
    auto filter = AddEntityComponent<MeshFilter>(entityId);
    filter->mesh = *mesh;
  }
  if (material) {
    auto renderer = AddEntityComponent<MeshRenderer>(entityId);
    renderer->material = *material;
  }
  if (boneData) {
    auto bones = AddEntityComponent<BoneData>(entityId);
    *bones = *boneData;
  }
  ForEachChildAsset(assetId, [this, entityId](const ID childAssetId) {
    auto name = GetAssetName(childAssetId);
    Instantiate(name, entityId);
  });
  return entityId;
}
void Editor::InstantiateRandom(const std::string& name, size_t count, float radius) {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  for (auto i = 0; i < count; ++i) {
    std::uniform_real_distribution<float> dist(.0f, 1.0f);
    auto theta = dist(gen) * 2.0f * glm::pi<float>();
    auto phi = acos(2.0f * dist(gen) - 1.0f);
    auto u = dist(gen);
    auto x = sin(phi) * cos(theta);
    auto y = sin(phi) * sin(theta);
    auto z = cos(phi);
    glm::vec3 direction(x, y, z);
    auto r = radius * std::cbrt(u);
    auto position = direction * r;
    auto nameTemp = name;
    Instantiate(nameTemp, ID::Invalid(), position);
  }
}
