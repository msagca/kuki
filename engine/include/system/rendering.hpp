#pragma once
#include "system.hpp"
#include <component/texture.hpp>
#include <entity_manager.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <kuki_engine_export.h>
#include <unordered_map>
#include <utility/octree.hpp>
#include <utility/pool.hpp>
namespace kuki {
enum class GizmoType : uint8_t {
  Manipulator,
  ViewFrustum,
  FrustumCulling
};
enum class GizmoMask : size_t {
  Manipulator = static_cast<size_t>(1) << static_cast<uint8_t>(GizmoType::Manipulator),
  ViewFrustum = static_cast<size_t>(1) << static_cast<uint8_t>(GizmoType::ViewFrustum),
  FrustumCulling = static_cast<size_t>(1) << static_cast<uint8_t>(GizmoType::FrustumCulling),
};
class Application;
class KUKI_ENGINE_API RenderingSystem final : public System {
private:
  std::unordered_map<MaterialType, Shader*> shaders;
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
  /// @param cubeMap Whether the texture is a cubemap or not, default: false
  /// @return true if the framebuffer is complete, false otherwise
  bool UpdateAttachments(unsigned int&, unsigned int&, unsigned int&, int, int, int = 1, bool = false);
  void DrawAsset(const Transform*, const Mesh*, const Material*);
  void DrawAsset(unsigned int);
  void DrawEntitiesInstanced(const Mesh*, const std::vector<unsigned int>&);
  void DrawFrustumCulling();
  void DrawGizmos();
  void DrawScene();
  void DrawSkybox(const Skybox* skybox);
  /// @brief Draws a skybox asset by applying equirectangular projection to it
  void DrawSkyboxAsset(unsigned int);
  void DrawViewFrustum();
  static unsigned int CreatePooledTexture();
  static void DeletePooledTexture(unsigned int);
  glm::mat4 GetEntityWorldTransform(const Transform*);
  glm::mat4 GetAssetWorldTransform(const Transform*);
  glm::vec3 GetAssetWorldPosition(const Transform*);
  BoundingBox GetAssetBounds(unsigned int);
  Shader* GetShader(MaterialType);
public:
  RenderingSystem(Application&);
  ~RenderingSystem();
  void Start() override;
  void Shutdown() override;
  int RenderSceneToTexture(Camera* = nullptr);
  int RenderAssetToTexture(unsigned int, int);
  Texture CreateCubeMapFromEquirect(Texture);
  Texture CreateIrradianceMapFromCubeMap(Texture);
  unsigned int GetGizmoMask() const;
  void SetGizmoMask(unsigned int);
  static void ToggleWireframeMode();
};
} // namespace kuki
