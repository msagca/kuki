#pragma once
#include "component/transform.hpp"
#include <asset_manager.hpp>
#include <camera_controller.hpp>
#include <entity_manager.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <system.hpp>
#include <unordered_map>
class RenderSystem : ISystem {
private:
  EntityManager& entityManager;
  AssetManager& assetManager;
  CameraController& cameraController;
  bool wireframeMode = false;
  unsigned int defaultShader;
  unsigned int fbo;
  unsigned int rbo;
  unsigned int colorTexture;
  bool ResizeBuffers(int, int);
  glm::mat4 GetWorldTransform(const Transform*);
  void DrawObjects();
  void DrawGizmos(int);
  void SetUniformLocations(unsigned int);
  std::unordered_map<unsigned int, std::unordered_map<unsigned int, int>> shaderToUniform;
public:
  RenderSystem(EntityManager&, AssetManager&, CameraController&);
  void ToggleWireframeMode();
  void Update() override;
  void CleanUp() override;
  int RenderToTexture(int, int, int);
};
