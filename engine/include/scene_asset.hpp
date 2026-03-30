#pragma once
#include <asset.hpp>
#include <bounding_box.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <material_fallback.hpp>
#include <mesh.hpp>
#include <string>
#include <texture.hpp>
namespace kuki {
struct SceneNode {
  std::string name;
  glm::mat4 transform{};
  std::vector<unsigned int> meshes;
  std::vector<unsigned int> children;
  int parent{-1};
  BoundingBox bounds{};
};
struct SceneMaterial {
  MaterialFallback fallback;
  std::vector<unsigned int> textures;
  std::string name;
  EntityID resourceId{};
};
struct SceneMesh {
  Mesh mesh;
  std::string name;
  unsigned int material;
  unsigned int parent;
  EntityID resourceId{};
};
struct SceneTexture {
  Texture texture;
  std::string name;
  EntityID resourceId{};
};
/// @brief Asset representation of an Assimp scene
struct KUKI_ENGINE_API SceneAsset final : public Asset {
  SceneAsset(AssetID, std::string = "");
  std::vector<SceneNode> nodes;
  std::vector<SceneMaterial> materials;
  std::vector<SceneMesh> meshes;
  std::vector<SceneTexture> textures;
};
} // namespace kuki
