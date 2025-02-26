#pragma once
#include <component/transform.hpp>
#include <engine_export.h>
#include <entity_manager.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <system.hpp>
class ENGINE_API RenderSystem : public System {
private:
  EntityManager assetManager;
  bool wireframeMode = false;
  Camera assetCam;
  Shader phongShader;
  Shader pbrShader;
  Shader unlitShader;
  unsigned int sceneFBO = 0;
  unsigned int sceneRBO = 0;
  unsigned int assetFBO = 0;
  unsigned int assetRBO = 0;
  unsigned int sceneTexture = 0;
  Scene* activeScene = nullptr;
  bool ResizeBuffers(unsigned int&, unsigned int&, unsigned int&, int, int);
  glm::mat4 GetWorldTransform(const Transform*);
  void DrawObject(const Transform*, const Mesh&, const Material&, const Camera&, EntityManager&);
  void DrawScene(const Camera&);
  void DrawAsset(unsigned int);
  void DrawGizmos(const Camera&, int = -1);
public:
  RenderSystem(EntityManager&);
  void ToggleWireframeMode();
  void Start() override;
  void Update(float, Scene*) override;
  void Shutdown() override;
  int RenderSceneToTexture(const Camera&, int, int, int = -1);
  bool RenderAssetToTexture(unsigned int, unsigned int, int);
};
