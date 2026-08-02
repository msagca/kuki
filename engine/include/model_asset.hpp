#pragma once
#include <asset.hpp>
#include <bounding_box.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <material_fallback.hpp>
#include <material_type.hpp>
#include <mesh.hpp>
#include <string>
#include <texture.hpp>
namespace kuki {
struct ModelNode {
  BoundingBox bounds{};
  glm::mat4 transform{};
  int parent{-1};
  std::string name;
  std::vector<unsigned int> children;
  std::vector<unsigned int> meshes;
};
struct ModelMaterial {
  EntityID resourceId{};
  MaterialFallback fallback;
  MaterialType type{MaterialType::Unlit};
  std::string name;
  std::vector<unsigned int> textures;
};
struct Bone {
  std::string name;
  unsigned int nodeIndex{};
  glm::mat4 offsetMatrix{1.f};
};
struct ModelMesh {
  EntityID resourceId{};
  Mesh mesh;
  std::string name;
  unsigned int material;
  unsigned int parent;
  std::vector<Bone> bones;
};
struct ModelTexture {
  EntityID resourceId{};
  Texture texture;
  std::string name;
};
struct PositionKey {
  float time{};
  glm::vec3 value{};
};
struct RotationKey {
  float time{};
  glm::quat value{};
};
struct ScaleKey {
  float time{};
  glm::vec3 value{1.f};
};
struct AnimationChannel {
  unsigned int nodeIndex{};
  std::string nodeName;
  std::vector<PositionKey> positions;
  std::vector<RotationKey> rotations;
  std::vector<ScaleKey> scales;
};
struct AnimationClip {
  std::string name;
  float duration{};
  float ticksPerSecond{25.f};
  std::vector<AnimationChannel> channels;
};
struct KUKI_ENGINE_API ModelAsset final : public Asset {
  ModelAsset(AssetID);
  std::vector<ModelMaterial> materials;
  std::vector<ModelMesh> meshes;
  std::vector<ModelNode> nodes;
  std::vector<ModelTexture> textures;
  std::vector<AnimationClip> animations;
};
} // namespace kuki
