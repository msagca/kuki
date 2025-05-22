#pragma once
#include <glad/glad.h>
#include "system.hpp"
#include <component/texture.hpp>
#include <entity_manager.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <kuki_engine_export.h>
#include <unordered_map>
#include <utility/octree.hpp>
#include <utility/texture_pool.hpp>
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
  std::unordered_map<ComputeType, ComputeShader*> computes;
  Camera assetCam{};
  Camera* targetCamera{};
  TexturePool texturePool;
  std::unordered_map<unsigned int, unsigned int> assetToTexture;
  unsigned int framebuffer{0};
  unsigned int framebufferMulti{0};
  unsigned int renderbuffer{0};
  unsigned int renderbufferMulti{0};
  unsigned int materialVBO{0};
  unsigned int transformVBO{0};
  unsigned int gizmoMask{0};
  static bool wireframeMode;
  /// @brief Update framebuffer attachments
  /// @param framebuffer OpenGL ID of the framebuffer
  /// @param renderbuffer OpenGL ID of the renderbuffer that is the depth and stencil attachment of the framebuffer
  /// @param texture OpenGL ID of the texture that is the color attachment of the framebuffer
  /// @param params Texture parameters
  /// @return true if the framebuffer is complete, false otherwise
  bool UpdateAttachments(unsigned int, unsigned int, unsigned int, const TextureParams&);
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
  void ApplyPostProc(unsigned int, unsigned int, const TextureParams&);
  glm::mat4 GetEntityWorldTransform(const Transform*);
  glm::mat4 GetAssetWorldTransform(const Transform*);
  glm::vec3 GetAssetWorldPosition(const Transform*);
  BoundingBox GetAssetBounds(unsigned int);
  Shader* GetShader(MaterialType);
  ComputeShader* GetCompute(ComputeType);
public:
  RenderingSystem(Application&);
  void Start() override;
  void Shutdown() override;
  int RenderSceneToTexture(Camera* = nullptr);
  int RenderAssetToTexture(unsigned int, int);
  Texture CreateCubeMapFromEquirect(Texture);
  Texture CreateIrradianceMapFromCubeMap(Texture);
  Texture CreatePrefilterMapFromCubeMap(Texture);
  Texture CreateBRDF_LUT();
  unsigned int GetGizmoMask() const;
  void SetGizmoMask(unsigned int);
  static void ToggleWireframeMode();
};
} // namespace kuki
