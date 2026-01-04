#pragma once
#include <asset.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <kuki_engine_export.h>
#include <material_asset.hpp>
#include <mesh_asset.hpp>
namespace kuki {
struct SceneNode {
  std::string name{};
  glm::mat4 transform{};
  std::vector<size_t> meshes{};
  std::vector<size_t> children{};
  int parent{-1};
};
struct KUKI_ENGINE_API SceneAsset final : public Asset {
  SceneAsset(const AssetID = AssetID::Invalid);
  std::vector<SceneNode> nodes;
  std::vector<MeshAsset> meshes;
  std::vector<MaterialAsset> materials;
  std::vector<TextureAsset> textures;
};
} // namespace kuki
