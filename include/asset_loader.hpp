#pragma once
#include <asset_manager.hpp>
#include <string>
class AssetLoader {
private:
  AssetManager& assetManager;
public:
  AssetLoader(AssetManager&);
  unsigned int LoadShader(const std::string&, const std::string&, const std::string&);
  unsigned int LoadModel(const std::string&, const std::string&);
  unsigned int LoadTexture(const std::string&, const std::string&);
  unsigned int LoadMesh(const std::string&, const std::vector<Vertex>&);
  unsigned int LoadMesh(const std::string&, const std::vector<Vertex>&, const std::vector<unsigned int>&);
};
