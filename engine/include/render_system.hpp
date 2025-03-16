#pragma once
#include <application.hpp>
#include <engine_export.h>
#include <entity_manager.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <system.hpp>
#include <utility/pool.hpp>
class ENGINE_API RenderSystem : public System {
private:
  std::unordered_map<std::string, Shader*> shaders;
  Camera assetCam;
  Pool<unsigned int> texturePool;
  unsigned int assetFBO = 0;
  unsigned int assetRBO = 0;
  unsigned int sceneFBO = 0;
  unsigned int sceneMultiFBO = 0;
  unsigned int sceneMultiRBO = 0;
  unsigned int sceneMultiTexture = 0;
  unsigned int sceneRBO = 0;
  unsigned int sceneTexture = 0;
  unsigned int instanceVBO = 0;
  bool UpdateBuffers(unsigned int&, unsigned int&, unsigned int&, int, int, int = 1);
  void DrawAsset(const Transform*, const Mesh&, const Material&);
  void DrawEntity(const Transform*, const Mesh&, const Material&);
  void DrawEntitiesInstanced(const Mesh&, const std::vector<unsigned int>&);
  void DrawSkybox();
  void DrawScene();
  void DrawAsset(unsigned int);
  static unsigned int CreateTexture();
  static void DeleteTexture(unsigned int);
  glm::mat4 GetEntityWorldTransform(const Transform*);
  glm::mat4 GetAssetWorldTransform(const Transform*);
  glm::vec3 GetAssetWorldPosition(const Transform*);
  std::tuple<glm::vec3, glm::vec3> GetAssetBounds(unsigned int);
  /// <summary>
  /// Position the camera to fully capture the target within the given bounds
  /// </summary>
  void PositionCamera(Camera&, const glm::vec3&, const glm::vec3&);
  Shader* GetMaterialShader(const Material&);
public:
  RenderSystem(Application&);
  ~RenderSystem();
  void Start() override;
  // void Update(float, Scene*) override;
  void Shutdown() override;
  int RenderSceneToTexture(int, int);
  int RenderAssetToTexture(unsigned int, int);
  static void ToggleWireframeMode();
};
