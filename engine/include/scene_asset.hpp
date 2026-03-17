#pragma once
#include <asset.hpp>
#include <bounding_box.hpp>
#include <color_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <id.hpp>
#include <material_fallback.hpp>
#include <primitive.hpp>
#include <texture_type.hpp>
namespace kuki {
struct SceneNode {
  glm::mat4 transform{};
  int parent{-1};
  std::string name;
  std::vector<size_t> children;
  std::vector<size_t> meshes;
};
struct SceneMaterial {
  MaterialFallback fallback;
  std::string name;
  std::vector<size_t> textures;
  EntityID resourceId{};
};
struct SceneMesh {
  BoundingBox bounds{};
  size_t material;
  std::string name;
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  EntityID resourceId{};
};
struct SceneTexture {
  ColorSpace color{ColorSpace::sRGB};
  TextureContent content{TextureContent::Albedo};
  TextureType type{TextureType::UV2D};
  int channels{3};
  int height{1024};
  int width{1024};
  std::string name;
  std::vector<unsigned char> data;
  EntityID resourceId{};
};
/// @brief Asset representation of an Assimp scene
struct SceneAsset final : public Asset {
  SceneAsset(AssetID, std::string = "");
  std::vector<SceneNode> nodes;
  std::vector<SceneMaterial> materials;
  std::vector<SceneMesh> meshes;
  std::vector<SceneTexture> textures;
};
} // namespace kuki
