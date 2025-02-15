#pragma once
#include <asset_manager.hpp>
#include <component/transform.hpp>
#include <engine_export.h>
#include <entity_manager.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <system.hpp>
#include <unordered_map>
class ENGINE_API RenderSystem : public System {
private:
  AssetManager assetManager;
  bool wireframeMode = false;
  Camera assetCam;
  unsigned int defaultLit = 0;
  unsigned int defaultUnlit = 0;
  unsigned int sceneFBO = 0;
  unsigned int sceneRBO = 0;
  unsigned int assetFBO = 0;
  unsigned int assetRBO = 0;
  unsigned int sceneTexture = 0;
  Scene* activeScene = nullptr;
  bool ResizeSceneBuffers(int, int);
  bool ResizeAssetBuffers(unsigned int, int);
  glm::mat4 GetWorldTransform(const Transform*);
  void DrawObjects(Camera&);
  void DrawAsset(unsigned int);
  void DrawGizmos(Camera&, int = -1);
  void SetUniformLocations(unsigned int);
  std::unordered_map<unsigned int, std::unordered_map<unsigned int, int>> shaderToUniform;
public:
  RenderSystem(AssetManager&);
  void ToggleWireframeMode();
  void Start() override;
  void Update(float, Scene*) override;
  void Shutdown() override;
  int RenderSceneToTexture(Camera&, int, int, int = -1);
  bool RenderAssetToTexture(unsigned int, unsigned int, int);
};
