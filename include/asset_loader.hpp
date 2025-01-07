#pragma once
#include <asset_manager.hpp>
#include <assimp/scene.h>
#include <component_types.hpp>
#include <primitive.hpp>
#include <string>
#include <vector>
class AssetLoader {
private:
  AssetManager& assetManager;
  unsigned int LoadNode(aiNode*, const aiScene*, Transform* = nullptr);
public:
  AssetLoader(AssetManager&);
  int LoadShader(const std::string&, const std::string&, const std::string&);
  int LoadModel(const std::string&, const std::string&);
  unsigned int LoadMesh(const std::string&, const std::vector<Vertex>&);
  unsigned int LoadMesh(const std::string&, const std::vector<Vertex>&, const std::vector<unsigned int>&);
};
