#pragma once
#include "component/component.hpp"
#include "component/material.hpp"
#include <asset_manager.hpp>
#include <assimp/material.h>
#include <assimp/scene.h>
#include <primitive.hpp>
#include <string>
#include <unordered_map>
#include <vector>
class AssetLoader {
private:
  AssetManager& assetManager;
  unsigned int LoadNode(aiNode*, const aiScene*, int = -1, const std::string& = "");
  Material CreateMaterial(aiMaterial*, const std::string&);
  std::unordered_map<std::string, unsigned int> pathToID;
public:
  AssetLoader(AssetManager&);
  // NOTE: int return type is for functions that require a path to the asset, so if the read fails they can return -1
  int LoadShader(const std::string&, const std::string&, const std::string&);
  int LoadModel(const std::string&, const std::string&);
  int LoadMaterial(const std::string&, const std::string&, const std::string& = "", const std::string& = "");
  int LoadTexture(const std::string&, const std::string&, TextureType);
  unsigned int LoadMesh(const std::string&, const std::vector<Vertex>&);
  unsigned int LoadMesh(const std::string&, const std::vector<Vertex>&, const std::vector<unsigned int>&);
};
