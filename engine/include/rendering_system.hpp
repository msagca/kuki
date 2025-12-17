#pragma once
#include <entity_manager.hpp>
#include <framebuffer_pool.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <kuki_engine_export.h>
#include <octree.hpp>
#include <renderbuffer_pool.hpp>
#include <shader.hpp>
#include <spdlog/spdlog.h>
#include <system.hpp>
#include <texture.hpp>
#include <texture_pool.hpp>
#include <uniform_buffer_pool.hpp>
#include <unordered_map>
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
  friend class Shader;
private:
  std::unordered_map<MaterialType, Shader*> shaders;
  std::unordered_map<ComputeType, ComputeShader*> computes;
  Camera assetCam{};
  FramebufferPool framebufferPool;
  RenderbufferPool renderbufferPool;
  TexturePool texturePool;
  UniformBufferPool uniformBufferPool;
  std::unordered_map<ID, unsigned int> assetToTexture;
  Texture brdf{}; // NOTE: generate once and re-use
  size_t fps{};
  unsigned int materialVBO{0};
  unsigned int transformVBO{0};
  size_t gizmoMask{0};
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
  void DrawAsset(ID);
  void DrawAssetHierarchy(ID);
  void DrawEntitiesInstanced(const Camera*, const Mesh*, const std::vector<ID>&);
  void DrawFrustumCulling(const Camera*, const Camera*);
  void DrawGizmos(const Camera*, const Camera* = nullptr);
  void DrawScene(const Camera*, const Camera* = nullptr);
  void DrawSkybox(const Camera*, const Skybox*);
  /// @brief Draws a skybox asset by applying equirectangular projection to it
  void DrawSkyboxAsset(ID);
  void DrawViewFrustum(const Camera*, const Camera*);
  void ApplyPostProc(unsigned int, unsigned int, const TextureParams&);
  BoundingBox GetAssetBounds(ID);
  Shader* GetShader(MaterialType);
  ComputeShader* GetCompute(ComputeType);
  void UpdateEntityTransforms();
  void UpdateCameraTransforms();
public:
  RenderingSystem(Application&);
  ~RenderingSystem();
  void Start() override;
  void Update(float) override;
  void LateUpdate(float) override;
  void Shutdown() override;
  size_t GetFPS() const;
  int RenderSceneToTexture(Camera* = nullptr);
  int RenderAssetToTexture(ID, int);
  Texture CreateCubeMapFromEquirect(Texture);
  Texture CreateIrradianceMapFromCubeMap(Texture);
  Texture CreatePrefilterMapFromCubeMap(Texture);
  Texture CreateBRDF_LUT();
  size_t GetGizmoMask() const;
  void SetGizmoMask(size_t);
  static void ToggleWireframeMode();
};
#include <rendering_system.inl>
} // namespace kuki
