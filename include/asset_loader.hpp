#pragma once
#include <asset_manager.hpp>
#include <assimp/scene.h>
#include <primitive.hpp>
#include <string>
#include <vector>
class AssetLoader {
private:
  AssetManager& assetManager;
  unsigned int LoadNode(aiNode*, const aiScene*, int = -1);
public:
  AssetLoader(AssetManager&);
  // NOTE: int return type is for functions that require a path to the asset, so if the read fails they can return -1
  int LoadShader(const std::string&, const std::string&, const std::string&);
  int LoadModel(const std::string&, const std::string&);
  unsigned int LoadMesh(const std::string&, const std::vector<Vertex>&);
  unsigned int LoadMesh(const std::string&, const std::vector<Vertex>&, const std::vector<unsigned int>&);
};
