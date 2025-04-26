#pragma once
#include "system.hpp"
#include <component/texture.hpp>
#include <entity_manager.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <kuki_export.h>
#include <unordered_map>
#include <utility/octree.hpp>
#include <utility/pool.hpp>
namespace kuki {
enum class GizmoID : uint8_t {
  Manipulator,
  ViewFrustum,
  FrustumCulling
};
enum class GizmoMask : size_t {
  Manipulator = static_cast<size_t>(1) << static_cast<uint8_t>(GizmoID::Manipulator),
  ViewFrustum = static_cast<size_t>(1) << static_cast<uint8_t>(GizmoID::ViewFrustum),
  FrustumCulling = static_cast<size_t>(1) << static_cast<uint8_t>(GizmoID::FrustumCulling),
};
class Application;
class KUKI_API RenderingSystem final : public System {
private:
  std::unordered_map<std::string, Shader*> shaders;
  Camera assetCam{};
  Camera* targetCamera{};
  Pool<unsigned int> texturePool;
  std::unordered_map<unsigned int, unsigned int> assetToTexture;
  unsigned int framebufferMulti{};
  unsigned int framebuffer{};
  unsigned int renderbufferAsset{};
  unsigned int renderbufferSceneMulti{};
  unsigned int renderbufferScene{};
  unsigned int textureSceneMulti{};
  unsigned int textureScene{};
  unsigned int materialVBO{};
  unsigned int transformVBO{};
  unsigned int gizmoMask{};
  /// @brief Initialize and/or resize the given buffers, set framebuffer attachments
  /// @param framebuffer ID of the framebuffer
  /// @param renderbuffer ID of the renderbuffer that is the depth and stencil attachment of the framebuffer
  /// @param texture ID of the texture that is the color attachment of the framebuffer
  /// @param width Width of the attachments
  /// @param height Height of the attachments
  /// @param samples Number of samples for multisampling, default: 1 (no multisampling)
  /// @return true if the framebuffer is complete, false otherwise
  bool UpdateAttachments(unsigned int&, unsigned int&, unsigned int&, int, int, int = 1);
  void DrawAsset(const Transform*, const Mesh&, const Material&);
  void DrawEntitiesInstanced(const Mesh&, const std::vector<unsigned int>&);
  void DrawSkybox(unsigned int);
  void DrawScene();
  void DrawAsset(unsigned int);
  /// @brief Draws a skybox asset by applying equirectangular projection to it
  void DrawSkyboxFlat(unsigned int);
  void DrawGizmos();
  void DrawFrustumCulling();
  void DrawViewFrustum();
  static unsigned int CreatePooledTexture();
  static void DeletePooledTexture(unsigned int);
  glm::mat4 GetEntityWorldTransform(const Transform*);
  glm::mat4 GetAssetWorldTransform(const Transform*);
  glm::vec3 GetAssetWorldPosition(const Transform*);
  BoundingBox GetAssetBounds(unsigned int);
  Shader* GetMaterialShader(const Material&);
public:
  RenderingSystem(Application&);
  ~RenderingSystem();
  void Start() override;
  // void Update(double, Scene*) override;
  void Shutdown() override;
  int RenderSceneToTexture(Camera* = nullptr);
  int RenderAssetToTexture(unsigned int, int);
  int RenderRadianceToCubeMap(unsigned int);
  unsigned int GetGizmoMask() const;
  void SetGizmoMask(unsigned int);
  static void ToggleWireframeMode();
};
} // namespace kuki
