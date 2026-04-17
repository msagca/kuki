#pragma once
#include <asset.hpp>
#include <bounding_box.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <material_fallback.hpp>
#include <material_type.hpp>
#include <mesh.hpp>
#include <string>
#include <texture.hpp>
namespace kuki {
struct SceneNode {
  BoundingBox bounds{};
  glm::mat4 transform{};
  int parent{-1};
  std::string name;
  std::vector<unsigned int> children;
  std::vector<unsigned int> meshes;
};
struct SceneMaterial {
  EntityID resourceId{};
  MaterialFallback fallback;
  MaterialType type{MaterialType::Unlit};
  std::string name;
  std::vector<unsigned int> textures;
};
struct SceneMesh {
  EntityID resourceId{};
  Mesh mesh;
  std::string name;
  unsigned int material;
  unsigned int parent;
};
struct SceneTexture {
  EntityID resourceId{};
  Texture texture;
  std::string name;
};
/// @brief Asset representation of an Assimp scene
struct KUKI_ENGINE_API SceneAsset final : public Asset {
  SceneAsset(AssetID, std::string = "");
  std::vector<SceneMaterial> materials;
  std::vector<SceneMesh> meshes;
  std::vector<SceneNode> nodes;
  std::vector<SceneTexture> textures;
};
} // namespace kuki
