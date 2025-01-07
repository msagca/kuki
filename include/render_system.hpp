#pragma once
#include <uniform_location.hpp>
#include <asset_loader.hpp>
#include <asset_manager.hpp>
#include <component_types.hpp>
#include <entity_manager.hpp>
#include <system.hpp>
#include <unordered_map>
class RenderSystem : ISystem {
private:
  EntityManager& entityManager;
  AssetLoader& assetLoader;
  AssetManager& assetManager;
  Camera* camera = nullptr;
  bool wireframeMode = false;
  unsigned int gridShader;
  unsigned int wireframeShader;
  unsigned int defaultShader;
  Mesh gridMesh;
  void RenderGrid();
  void RenderObjects();
  void SetUniformLocations(unsigned int);
  std::unordered_map<unsigned int, std::unordered_map<unsigned int, int>> shaderToUniform;
public:
  RenderSystem(EntityManager&, AssetManager&, AssetLoader&);
  void SetCamera(Camera*);
  void ToggleWireframeMode();
  void Update() override;
};
