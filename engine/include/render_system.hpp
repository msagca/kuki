#pragma once
#include <engine_export.h>
#include <entity_manager.hpp>
#include <functional>
#include <glm/ext/matrix_float4x4.hpp>
#include <system.hpp>
class ENGINE_API RenderSystem : public System {
private:
  inline static const auto CLEAR_COLOR = glm::vec4(.1f, .1f, .1f, 1.0f);
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
  std::function<void(const Camera&, Transform*)> gizmoDrawCallback;
public:
  RenderSystem(EntityManager&);
  void ToggleWireframeMode();
  void Start() override;
  void Update(float, Scene*) override;
  void Shutdown() override;
  int RenderSceneToTexture(const Camera&, int, int, int = -1);
  bool RenderAssetToTexture(unsigned int, unsigned int, int);
  void SetGizmoDrawCallback(std::function<void(const Camera&, Transform*)>);
};
