#pragma once
#include <asset_manager.hpp>
#include <string>
class AssetLoader {
private:
  AssetManager& assetManager;
public:
  AssetLoader(AssetManager&);
  void LoadModel(const std::string&);
  void LoadTexture(const std::string&);
};
