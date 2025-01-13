#pragma once
#include <asset_loader.hpp>
#include <asset_manager.hpp>
#include <camera_controller.hpp>
#include <component_types.hpp>
#include <entity_manager.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <system.hpp>
#include <unordered_map>
class RenderSystem : ISystem {
private:
  EntityManager& entityManager;
  AssetLoader& assetLoader;
  AssetManager& assetManager;
  CameraController& cameraController;
  bool wireframeMode = false;
  unsigned int gridShader;
  unsigned int wireframeShader;
  unsigned int defaultShader;
  Mesh gridMesh;
  glm::mat4 GetWorldTransform(const Transform*);
  void RenderGrid();
  void RenderObjects();
  void SetUniformLocations(unsigned int);
  std::unordered_map<unsigned int, std::unordered_map<unsigned int, int>> shaderToUniform;
public:
  RenderSystem(EntityManager&, AssetManager&, AssetLoader&, CameraController&);
  void ToggleWireframeMode();
  void Update() override;
};
