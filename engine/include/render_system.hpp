#pragma once
#include <kuki_export.h>
#include <entity_manager.hpp>
#include <component/texture.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <system.hpp>
#include <utility/octree.hpp>
#include <utility/pool.hpp>
enum class GizmoID : unsigned int {
  Manipulator,
  ViewFrustum,
  FrustumCulling
};
enum class GizmoMask : unsigned int {
  Manipulator = 1 << static_cast<unsigned int>(GizmoID::Manipulator),
  ViewFrustum = 1 << static_cast<unsigned int>(GizmoID::ViewFrustum),
  FrustumCulling = 1 << static_cast<unsigned int>(GizmoID::FrustumCulling),
};
class Application;
class KUKI_API RenderSystem final : public System {
private:
  std::unordered_map<std::string, Shader*> shaders;
  Camera assetCam;
  Camera* targetCamera;
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
  unsigned int gizmoMask = 0;
  bool UpdateBuffers(unsigned int&, unsigned int&, unsigned int&, int, int, int = 1);
  void DrawAsset(const Transform*, const Mesh&, const Material&);
  void DrawEntity(const Transform*, const Mesh&, const Material&);
  void DrawEntitiesInstanced(const Mesh&, const std::vector<unsigned int>&);
  void DrawSkybox();
  void DrawScene();
  void DrawAsset(unsigned int);
  /// @brief Draws a skybox asset by applying equirectangular projection to it
  void DrawSkyboxFlat(unsigned int);
  void DrawGizmos();
  void DrawFrustumCulling();
  void DrawViewFrustum();
  static unsigned int CreateTexture();
  static void DeleteTexture(unsigned int);
  glm::mat4 GetEntityWorldTransform(const Transform*);
  glm::mat4 GetAssetWorldTransform(const Transform*);
  glm::vec3 GetAssetWorldPosition(const Transform*);
  BoundingBox GetAssetBounds(unsigned int);
  Shader* GetMaterialShader(const Material&);
public:
  RenderSystem(Application&);
  ~RenderSystem();
  void Start() override;
  // void Update(float, Scene*) override;
  void Shutdown() override;
  int RenderSceneToTexture(Camera* = nullptr);
  int RenderAssetToTexture(unsigned int, int);
  unsigned int GetGizmoMask() const;
  void SetGizmoMask(unsigned int);
  static void ToggleWireframeMode();
};
