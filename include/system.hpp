#pragma once
#include <asset_manager.hpp>
#include <asset_loader.hpp>
#include <component_types.hpp>
#include <entity_manager.hpp>
#include <glad/glad.h>
class ISystem {
public:
  virtual ~ISystem() = default;
  virtual void Update() = 0;
};
class RenderSystem : ISystem {
private:
  EntityManager& entityManager;
  AssetLoader& assetLoader;
  AssetManager& assetManager;
  Camera* camera;
  bool wireframeMode = false;
  unsigned int gridShader;
  unsigned int wireframeShader;
  unsigned int defaultShader;
  Mesh gridMesh;
  void RenderGrid();
  void RenderObjects();
public:
  RenderSystem(EntityManager&, AssetManager&, AssetLoader&);
  void SetCamera(Camera*);
  void ToggleWireframeMode();
  void Update() override;
};
