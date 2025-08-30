#pragma once
#include <glad/glad.h>
#include "system.hpp"
#include <component/shader.hpp>
#include <component/texture.hpp>
#include <entity_manager.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <kuki_engine_export.h>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <utility/framebuffer_pool.hpp>
#include <utility/octree.hpp>
#include <utility/renderbuffer_pool.hpp>
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
  FramebufferPool framebufferPool;
  RenderbufferPool renderbufferPool;
  TexturePool texturePool;
  std::unordered_map<unsigned int, unsigned int> assetToTexture;
  int fps{};
  unsigned int materialVBO{0};
  unsigned int transformVBO{0};
  unsigned int gizmoMask{0};
  static bool wireframeMode;
  /// @brief Update framebuffer attachments
  /// @param params Texture parameters
  /// @param framebuffer OpenGL ID of the framebuffer
  /// @param renderbuffer OpenGL ID of the renderbuffer that is the depth and stencil attachment
  /// @param textures OpenGL IDs of the textures that are the color attachments
  /// @return true if the framebuffer is complete, false otherwise
  template <typename... T>
  requires(std::same_as<T, unsigned int> && ...)
  bool UpdateAttachments(const TextureParams&, unsigned int, unsigned int, T...);
  void DrawAsset(unsigned int);
  void DrawAssetHierarchy(unsigned int);
  void DrawEntitiesInstanced(const Camera*, const Mesh*, const std::vector<unsigned int>&);
  void DrawFrustumCulling(const Camera*, const Camera*);
  void DrawGizmos(const Camera*, const Camera* = nullptr);
  void DrawScene(const Camera*, const Camera* = nullptr);
  void DrawSkybox(const Camera*, const Skybox*);
  /// @brief Draws a skybox asset by applying equirectangular projection to it
  void DrawSkyboxAsset(unsigned int);
  void DrawViewFrustum(const Camera*, const Camera*);
  void ApplyPostProc(unsigned int, unsigned int, const TextureParams&);
  BoundingBox GetAssetBounds(unsigned int);
  Shader* GetShader(MaterialType);
  ComputeShader* GetCompute(ComputeType);
  // TODO: the following should be handled by a physics system
  void UpdateTransforms();
  void UpdateChildFlags(unsigned int);
public:
  RenderingSystem(Application&);
  ~RenderingSystem();
  void Start() override;
  void Update(float) override;
  void Shutdown() override;
  int GetFPS() const;
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
template <typename... T>
requires(std::same_as<T, unsigned int> && ...)
bool RenderingSystem::UpdateAttachments(const TextureParams& params, unsigned int framebuffer, unsigned int renderbuffer, T... textures) {
  if (framebuffer == 0) {
    spdlog::error("Framebuffer is not initialized.");
    return false;
  }
  if (renderbuffer == 0) {
    spdlog::error("Renderbuffer attachment is not initialized.");
    return false;
  }
  auto checkTexture = [](auto texture) {
    if (texture == 0) {
      spdlog::error("Texture attachment is not initialized.");
      return false;
    }
    return true;
  };
  if (!(checkTexture(textures) && ...))
    return false;
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  auto attachment = GL_COLOR_ATTACHMENT0;
  const auto cubeMap = params.target == GL_TEXTURE_CUBE_MAP;
  auto target = cubeMap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : params.target;
  auto bindTexture = [target, &attachment](auto texture) mutable {
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, target, texture, 0);
    ++attachment;
  };
  (bindTexture(textures), ...);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
  auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    spdlog::error("Framebuffer is incomplete ({0:x}).", status);
    return false;
  }
  return true;
}
} // namespace kuki
