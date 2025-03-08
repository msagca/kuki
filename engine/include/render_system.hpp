#pragma once
#include <engine_export.h>
#include <entity_manager.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <system.hpp>
class ENGINE_API RenderSystem : public System {
private:
  EntityManager assetManager;
  std::unordered_map<std::string, Shader*> shaders;
  Camera assetCam;
  unsigned int sceneFBO = 0;
  unsigned int sceneRBO = 0;
  unsigned int assetFBO = 0;
  unsigned int assetRBO = 0;
  unsigned int sceneTexture = 0;
  Scene* activeScene = nullptr;
  Camera* activeCamera = nullptr;
  bool ResizeBuffers(unsigned int&, unsigned int&, unsigned int&, int, int);
  glm::mat4 GetWorldTransform(const Transform*);
  void DrawObject(const Transform*, const Mesh&, const Material&);
  void DrawSkybox();
  void DrawScene();
  void DrawAsset(unsigned int);
public:
  RenderSystem(EntityManager&);
  ~RenderSystem();
  void Start() override;
  void Update(float, Scene*) override;
  void Shutdown() override;
  int RenderSceneToTexture(int, int);
  bool RenderAssetToTexture(unsigned int, unsigned int, int);
  static void ToggleWireframeMode();
};
