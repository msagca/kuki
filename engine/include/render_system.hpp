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
  unsigned int fbo = 0;
  unsigned int rbo = 0;
  unsigned int colorTexture = 0;
  Scene* activeScene = nullptr;
  bool ResizeBuffers(int, int);
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
  int RenderAssetToTexture(unsigned int, int);
  int RenderSceneToTexture(Camera&, int, int, int = -1);
};
