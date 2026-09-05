#pragma once
#include <array>
#include <asset_manager.hpp>
#include <bounding_box.hpp>
#include <concepts.hpp>
#include <cstdint>
#include <gl_buffer.hpp>
#include <gl_buffer_pool.hpp>
#include <gl_compute_shader.hpp>
#include <gl_format.hpp>
#include <gl_framebuffer.hpp>
#include <gl_framebuffer_pool.hpp>
#include <gl_lit_shader.hpp>
#include <gl_mesh.hpp>
#include <gl_mesh_material.hpp>
#include <gl_render_target.hpp>
#include <gl_renderbuffer.hpp>
#include <gl_renderbuffer_pool.hpp>
#include <gl_resource_registry.hpp>
#include <gl_shader.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <gl_texture_pool.hpp>
#include <gl_unlit_shader.hpp>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <light.hpp>
#include <light_limits.hpp>
#include <material_asset.hpp>
#include <material_fallback.hpp>
#include <mesh_asset.hpp>
#include <model_asset.hpp>
#include <render_graph.hpp>
#include <renderer.hpp>
#include <rendering_system.hpp>
#include <scene_manager.hpp>
#include <shader_type.hpp>
#include <span>
#include <spdlog/spdlog.h>
#include <target_description.hpp>
#include <texture_asset.hpp>
#include <texture_content.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace kuki {
/// @brief One draw held back until every opaque surface has been rasterised.
///
/// Two unrelated kinds of surface end up here for the same reason. Blended ones cannot be drawn
/// where they are found because the result depends on the order they reach the blend unit.
/// Transmissive ones blend nothing, but they read the scene behind them out of a copy of the
/// colour buffer, and that copy is only complete once everything opaque has been drawn.
///
/// `blended` decides which of the two a draw is, and with it what state it is issued under:
/// blending on with depth writes off, or the ordinary opaque state. A transmissive surface wants
/// the opaque state, since it composites the background itself.
///
/// Instancing is given up for both: a batch is issued in one order, and a blended surface's
/// result depends on the order it reaches the blend unit, so each instance has to be sortable on
/// its own. Transparency is rare enough that one draw call each costs less than getting it wrong.
///
/// `depth` is the distance from the camera to the instance's origin, which is what the sort keys
/// on. That resolves objects against each other but not a single mesh against itself, whose own
/// faces still blend in index order.
struct GLDeferredDraw {
  const GLMesh *mesh{};
  const GLMaterial *material{};
  MaterialFallback fallback{};
  glm::mat4 transform{1.f};
  /// @brief The posed skeleton, for a skinned draw; empty for every other one.
  std::vector<glm::mat4> bones;
  uint32_t entityId{};
  bool blended{};
  float depth{};
};
/// @brief One instanced draw call's worth of work: a mesh, a material, and every instance of them.
///
/// Mirrors `DXDrawBatch`, minus its hidden instances: the OpenGL backend traces no rays, so an
/// instance the camera cannot see is dropped rather than kept aside.
///
/// The mesh and material point at the components the batch was grouped from rather than holding
/// copies, so a draw always uses what the entity carries this frame.
struct GLDrawBatch {
  const GLMesh *mesh{};
  const GLMaterial *material{};
  std::vector<glm::mat4> transforms;
  std::vector<MaterialFallback> fallbacks;
  std::vector<uint32_t> entityIds;
};
inline constexpr int ASSET_PREVIEW_SIZE = 128;
class KUKI_ENGINE_API GLRenderer final : public Renderer {
public:
  GLRenderer(Application &);
  auto ApplyAntiAliasing(std::span<std::string>, std::span<std::string>) -> void override;
  /// @brief Every pass below draws a fullscreen quad into its output and sets the viewport to match.
  ///
  /// Setting it is not optional even though the passes run back to back: the viewport is global
  /// state that outlives whatever bound it, so a pass that inherits one belonging to a differently
  /// sized target rasterises its quad at the wrong scale. That stays invisible for as long as every
  /// target in the graph happens to share the viewport's size, and breaks the moment one does not.
  auto ApplyBloomEffect(std::span<std::string>, std::span<std::string>) -> void override;
  /// @brief Blurs the first input into the first output through a chain of single-axis passes.
  ///
  /// A two-dimensional Gaussian separates into a horizontal and a vertical pass, so the passes
  /// alternate axes and ping-pong between two scratch targets named after the output. Running the
  /// pair repeatedly widens the blur far more cheaply than widening the kernel would.
  auto ApplyBlurEffect(std::span<std::string>, std::span<std::string>) -> void override;
  auto ApplyBrightPassFilter(std::span<std::string>, std::span<std::string>) -> void override;
  auto ApplyToneMapping(std::span<std::string>, std::span<std::string>) -> void override;
  /// @brief Draws the selection outline over the scene, or passes the scene through unchanged.
  ///
  /// Takes the colour to draw over from the first single-sampled input and the entity ids from the
  /// input that carries an id buffer, rather than by name, so the pass works wherever a graph puts
  /// it and whatever the pass feeding it is called.
  auto ApplyOutline(std::span<std::string>, std::span<std::string>) -> void override;
  auto BorrowBuffer(const int & = 0) -> unsigned int;
  auto BorrowFramebuffer() -> unsigned int;
  auto BorrowRenderbuffer(const TargetDescription &) -> unsigned int;
  auto BorrowTexture(const TargetDescription &) -> unsigned int;
  auto Clear() -> void override;
  auto CreateBuffer(const std::string & = "", const int & = 0) -> EntityID;
  auto CreateDepthPrepass(std::span<std::string>, std::span<std::string>) -> void override;
  auto CreateShadowMap(std::span<std::string>, std::span<std::string>) -> void override;
  auto CreateSpotShadowMap(std::span<std::string>, std::span<std::string>) -> void override;
  auto CreateTarget(const TargetDescription &, const std::string & = "") -> EntityID override;
  auto CreateTexture(const TargetDescription &, const std::string & = "") -> EntityID;
  auto DrawEntities(std::span<std::string>, const GLRenderTarget &) -> void;
  /// @brief Blits the colour buffer into the scene colour copy, mid-pass, and rebinds the buffer.
  ///
  /// Runs between the opaque draws and the deferred ones, which is the only moment the copy is both
  /// complete and not yet needed. Every deferred draw reads the same copy, so a transmissive
  /// surface behind another one refracts what was there before either was drawn.
  ///
  /// @return The texture to sample, or zero when the copy could not be made.
  auto CaptureSceneColor(const GLRenderTarget &) -> unsigned int;
  auto DrawEntitiesInstanced(std::span<std::string>, const GLMesh &, const GLMaterial &, const std::vector<MaterialFallback> &, const std::vector<glm::mat4> &, const std::vector<uint32_t> &, const unsigned int = 0) -> void;
  /// @brief Issues one skinned mesh, with its own bone palette.
  ///
  /// Split out of the traversal that finds skinned meshes so a deferred one can be posed where it
  /// is found and drawn later, once the scene behind it has been captured.
  ///
  /// @param sceneColor The scene colour copy a transmissive material refracts, or zero for none.
  auto DrawSkinnedMesh(std::span<std::string>, const GLMesh &, const GLMaterial &, std::span<const glm::mat4>, const uint32_t, const unsigned int = 0) -> void;
  auto DrawMeshes() -> void;
  auto DrawOpaqueMeshes(const glm::mat4 &, const glm::mat4 &) -> void;
  auto DrawMeshesInstanced(const GLMesh &, const std::vector<glm::mat4> &, const glm::mat4 &view, const glm::mat4 &projection) -> void;
  /// @brief Poses every skinned mesh, drawing the opaque ones and handing back the rest.
  auto DrawSkinnedEntities(std::span<std::string>, std::vector<GLDeferredDraw> &) -> void;
  auto DrawSkybox() -> void;
  auto GetBuffer(const EntityID) -> GLBuffer *;
  auto GetBuffer(const std::string &) -> GLBuffer *;
  auto GetCompute(const std::string &) -> GLComputeShader *;
  auto GetPreviewSize() const -> int override;
  auto GetPrimitive(const std::string &) -> GLMesh *;
  auto GetResourceID(const std::string &) const -> EntityID;
  auto GetScene(this auto &, const std::string & = "") -> decltype(auto);
  /// @brief Looks up a shader resource by name, trying each concrete shader component in turn.
  ///
  /// TODO: make `GetComponent<T>` work with derived types so `GLShader` can be used as the type argument directly
  auto GetShader(const std::string &) -> GLShader *;
  auto GetTarget(const EntityID) -> GLRenderTarget *;
  auto GetTarget(const std::string &) -> GLRenderTarget * override;
  auto GetTarget(std::span<std::string> names, const std::string &name) -> GLRenderTarget *;
  auto GetTexture(const EntityID) -> GLTexture *;
  auto GetTexture(const std::string &) -> GLTexture *;
  auto LoadAsset(Asset &) -> void;
  auto LoadAsset(const AssetID) -> void override;
  auto LoadAssets(const AssetType) -> void override;
  auto LoadScene(Scene &) -> void override;
  /// @brief Reads the entity ID written to the picking buffer at the given pixel.
  ///
  /// Only the low 24 bits (RGB) carry the ID. The shaders that write this buffer force alpha to 1.0 rather than packing a fourth ID byte, because global alpha blending would discard the write whenever that byte was 0 — which is the case for virtually every entity ID.
  ///
  /// @return The entity under the pixel, or an invalid ID when nothing was hit.
  auto PickEntity(const int, const int) -> EntityID override;
  auto PreviewAsset(const AssetID) -> GLTexture * override;
  auto RenderScene(std::span<std::string>, std::span<std::string>) -> void override;
  auto PresentTarget(const std::string &) -> void override;
  auto Reset() -> void override;
  auto SetPreviewSize(const int) -> void override;
  auto SetResolution(const int = 1920, const int = 1080) -> void override;
  auto GetPoolUsage() const -> PoolUsage override;
  auto TraceProbes(std::span<std::string>, std::span<std::string>) -> void override;
  auto UpdateTarget(const std::string &, const TargetDescription &) -> void override;
  template <typename T>
  auto GetComponent(this auto &, const EntityID) -> decltype(auto);
  template <typename T>
  auto GetComponent(this auto &, const std::string & = "") -> decltype(auto);
  template <IsAsset T>
  auto LoadAsset(T &) -> void;
  template <IsAsset T>
  auto PreviewAsset(T &) -> GLTexture *;
  template <AreUnsignedInt... Vals>
  auto ReturnBuffer(Vals &&...) -> void;
  template <AreUnsignedInt... Vals>
  auto ReturnFramebuffer(Vals &&...) -> void;
  template <AreUnsignedInt... Vals>
  auto ReturnRenderbuffer(const TargetDescription &, Vals &&...) -> void;
  template <AreUnsignedInt... Vals>
  auto ReturnTexture(const TargetDescription &, Vals &&...) -> void;
private:
  GLResourceRegistry resourceRegistry;
  auto BypassPass(const RenderPass, std::span<std::string>, std::span<std::string>) -> void override;
  auto BypassCopy(std::span<std::string>, std::span<std::string>) -> void override;
  auto BypassClear(std::span<std::string>) -> void override;
  /// @brief Frames since the pools were last given a chance to let go of what nobody is using.
  size_t framesSinceCollect{};
  GLBufferPool bufferPool;
  GLFramebufferPool framebufferPool;
  GLRenderbufferPool renderbufferPool;
  GLTexturePool texturePool;
  std::unordered_map<AssetID, EntityID> assetIdToResourceId;
  std::unordered_map<AssetID, EntityID> assetIdToPreviewId;
  /// @brief Storage `CollectBatches` refills rather than rebuilds, and the span it hands back.
  ///
  /// Same reasoning as `DXRenderer::batchPool`: clearing the per-batch arrays keeps their capacity,
  /// so a steady scene stops allocating here after its first frame. `batchPool` may hold more
  /// batches than the last call used; the returned span is what says how many are live.
  std::vector<GLDrawBatch> batchPool;
  std::unordered_map<GLMeshMat, size_t> batchLookup;
  std::unordered_set<EntityID> placeholderScaledMeshes;
  int previewSize{ASSET_PREVIEW_SIZE};
  glm::mat4 shadowView{1.f};
  glm::mat4 shadowProjection{1.f};
  std::array<glm::mat4, MAX_SPOT_SHADOW_LIGHTS> spotShadowViewProj{};
  unsigned int spotShadowCount{};
  unsigned int pickFramebuffer{};
  unsigned int pickTexture{};
  unsigned int outlineIdFramebuffer{};
  unsigned int outlineIdTexture{};
  int outlineIdWidth{};
  int outlineIdHeight{};
  /// @brief A single-sampled copy of the colour buffer as it stood when the opaque phase ended.
  ///
  /// What a transmissive surface refracts. A framebuffer cannot be sampled while it is being drawn
  /// into, so the scene behind such a surface has to be blitted out to a texture of its own first.
  /// The blit resolves the multisampled scene buffer in the same step.
  ///
  /// Carries no mip chain, so a rough transmissive surface refracts as sharply as a smooth one.
  /// This matches the Direct3D backend, which cannot generate one as cheaply and so does without.
  unsigned int sceneColorFramebuffer{};
  unsigned int sceneColorTexture{};
  int sceneColorWidth{};
  int sceneColorHeight{};
  static auto CreateIndexBuffer(GLMesh &, const std::vector<unsigned int> &) -> void;
  static auto CreateVertexBuffer(GLMesh &, const std::vector<Vertex> &, bool = false) -> void;
  auto ConvertCubemapToEquirectangularMap(const TargetDescription &, const unsigned int) -> unsigned int;
  auto ConvertEquirectangularMapToCubemap(const TargetDescription &, const unsigned int, bool = false) -> unsigned int;
  auto CreateBRDF_LUT(const TargetDescription &) -> unsigned int;
  auto CreateIrradianceMap(const TargetDescription &, const unsigned int, const unsigned int) -> unsigned int;
  /// @brief Prefilters a skybox cubemap into the split-sum specular map used for image-based lighting.
  ///
  /// The mip count set here is what `MAX_REFLECTION_LOD` in `lit.frag` must stay one less than; the shader has no way to query it.
  auto CreatePrefilterMap(const TargetDescription &, const unsigned int) -> unsigned int;
  auto FitShadowFrustum(Scene &, const Light &) -> void;
  static auto FitSpotShadowFrustum(const Light &, glm::mat4 &view, glm::mat4 &projection) -> void;
  auto GetAssetPreviewId(const AssetID) const -> EntityID;
  auto GetAssetResourceId(const AssetID) const -> EntityID;
  auto GetDefaultMaterialAssetId() -> AssetID;
  auto GetDefaultMeshAssetId() -> AssetID;
  auto LoadMesh(Mesh &, bool = false) -> GLMesh;
  auto UpdatePlaceholderScale(Scene &, const EntityID, const bool pending) -> void;
  auto LoadModelMaterial(ModelAsset &, const size_t) -> EntityID;
  auto LoadModelMesh(ModelAsset &, const size_t) -> EntityID;
  auto LoadModelTexture(ModelAsset &, const size_t) -> GLTexture *;
  auto LoadTexture(Texture &) -> GLTexture;
  auto LoadCompressedTexture(Texture &, GLTexture &) -> GLTexture;
  /// @brief Groups the scene's drawable meshes into one batch per mesh and material.
  ///
  /// The counterpart of `DXRenderer::CollectBatches`, and grouped on the same rule: two entities
  /// share a draw call only where `GLMeshMat` says their mesh and material are interchangeable.
  ///
  /// Regrouped every frame rather than cached against the structural generation. The cache it
  /// replaced was not free — it held the id of every entity in a bucket, and a frame had to fetch
  /// that entity's transform, bounds and material back by id, which cost more than the regrouping
  /// it saved. Reading them out of the archetype's columns instead is both faster and always
  /// current, where the cached bucket keys held copies that a mesh or texture reload left stale.
  ///
  /// @param camera Drops what it cannot see. Null keeps everything, for a pass with no view of
  /// its own.
  ///
  /// @warning The result views storage this reuses on the next call, so only one collection may be
  /// live at a time.
  auto CollectBatches(Scene &, const Camera *) -> std::span<const GLDrawBatch>;
};
template <typename T>
auto GLRenderer::GetComponent(this auto &self, const EntityID id) -> decltype(auto) {
  return self.resourceRegistry.template GetComponent<T>(id);
}
template <typename T>
auto GLRenderer::GetComponent(this auto &self, const std::string &name) -> decltype(auto) {
  if (name.empty())
    return self.resourceRegistry.template GetAny<T>();
  return self.resourceRegistry.template GetComponent<T>(name);
}
auto GLRenderer::GetScene(this auto &self, const std::string &name) -> decltype(auto) {
  return self.app.GetScene(name);
}
template <AreUnsignedInt... Vals>
auto GLRenderer::ReturnBuffer(Vals &&...vals) -> void {
  bufferPool.Release(vals...);
}
template <AreUnsignedInt... Vals>
auto GLRenderer::ReturnFramebuffer(Vals &&...vals) -> void {
  framebufferPool.Release(vals...);
}
template <AreUnsignedInt... Vals>
auto GLRenderer::ReturnRenderbuffer(const TargetDescription &desc, Vals &&...vals) -> void {
  renderbufferPool.Release(desc, vals...);
}
template <AreUnsignedInt... Vals>
auto GLRenderer::ReturnTexture(const TargetDescription &desc, Vals &&...vals) -> void {
  texturePool.Release(desc, vals...);
}
template <IsAsset T>
auto GLRenderer::LoadAsset(T &) -> void {}
template <>
inline auto GLRenderer::LoadAsset<MaterialAsset>(MaterialAsset &materialAsset) -> void {
  if (resourceRegistry.IsEntity(GetAssetResourceId(materialAsset.id)))
    return;
  const auto name = app.GetAssetName(materialAsset.id);
  const auto id = resourceRegistry.Create(name);
  assetIdToResourceId[materialAsset.id] = id;
  auto glMaterial = resourceRegistry.AddComponent<GLMaterial>(id);
  glMaterial->fallback = materialAsset.fallback;
  glMaterial->type = materialAsset.type;
  for (const auto &id : materialAsset.textures) {
    LoadAsset(id);
    const auto textureAsset = app.GetAsset<TextureAsset>(id);
    const auto texture = resourceRegistry.GetComponent<GLTexture>(GetAssetResourceId(textureAsset->id));
    switch (texture->content) {
    case TextureContent::Emissive:
      glMaterial->textures.emissive = texture->id;
      break;
    case TextureContent::Metalness:
      glMaterial->textures.metalness = texture->id;
      break;
    case TextureContent::Normal:
      glMaterial->textures.normal = texture->id;
      break;
    case TextureContent::Occlusion:
      glMaterial->textures.occlusion = texture->id;
      break;
    case TextureContent::Roughness:
      glMaterial->textures.roughness = texture->id;
      break;
    case TextureContent::Specular:
      glMaterial->textures.specular = texture->id;
      break;
    default:
      glMaterial->textures.albedo = texture->id;
      break;
    }
  }
  spdlog::info("[GLRenderer] loaded material: {}", name);
}
template <>
inline auto GLRenderer::LoadAsset<MeshAsset>(MeshAsset &meshAsset) -> void {
  if (resourceRegistry.IsEntity(GetAssetResourceId(meshAsset.id)))
    return;
  const auto name = app.GetAssetName(meshAsset.id);
  const auto id = resourceRegistry.Create(name);
  assetIdToResourceId[meshAsset.id] = id;
  auto glMesh = resourceRegistry.AddComponent<GLMesh>(id);
  *glMesh = LoadMesh(meshAsset.mesh);
  LoadAsset(meshAsset.material);
  spdlog::info("[GLRenderer] loaded mesh: {}", name);
}
template <>
inline auto GLRenderer::LoadAsset<ModelAsset>(ModelAsset &modelAsset) -> void {
  // TODO: check if the asset is already loaded (individual materials and textures are already checked)
  // NOTE: materials and textures are loaded in the process if they are referenced by any mesh
  for (auto i = 0; i < modelAsset.meshes.size(); ++i)
    LoadModelMesh(modelAsset, i);
}
template <>
inline auto GLRenderer::LoadAsset<ShaderAsset>(ShaderAsset &shaderAsset) -> void {
  if (resourceRegistry.IsEntity(GetAssetResourceId(shaderAsset.id)))
    return;
  const auto name = app.GetAssetName(shaderAsset.id);
  if (shaderAsset.shaderType == ShaderType::Compute) {
    auto &compShader = shaderAsset;
    const auto id = resourceRegistry.Create(name);
    assetIdToResourceId[compShader.id] = id;
    auto compute = resourceRegistry.AddComponent<GLComputeShader>(id);
    auto compId = GLShaderBase::Compile(compShader.text.data(), GL_COMPUTE_SHADER);
    compShader.text = {};
    const auto programId = glCreateProgram();
    compute->id = programId;
    glAttachShader(programId, compId);
    glLinkProgram(programId);
    glDeleteShader(compId);
    int success;
    glGetProgramiv(programId, GL_LINK_STATUS, &success);
    if (success) {
      compute->CacheLocations();
      spdlog::info("[GLRenderer] created compute: {}", name);
    } else {
      int logLength{};
      glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &logLength);
      std::string log(logLength, '\0');
      if (logLength > 0)
        glGetProgramInfoLog(programId, logLength, nullptr, log.data());
      spdlog::error("[GLRenderer] failed to create compute: {}\n{}", name, log);
    }
  } else if (shaderAsset.shaderType == ShaderType::Fragment) {
    auto &fragShader = shaderAsset;
    auto vertAsset = app.GetAsset(fragShader.vertexShader);
    if (!vertAsset) {
      spdlog::warn("[GLRenderer] Vertex shader for the following fragment shader could not be found: {}", name);
      return;
    }
    auto vertShader = vertAsset->As<ShaderAsset>();
    const auto vertName = app.GetAssetName(fragShader.vertexShader);
    if (!vertShader) {
      spdlog::error("[GLRenderer] Asset is not a vertex shader: {}", vertName);
      return;
    }
    const auto id = resourceRegistry.Create(name);
    assetIdToResourceId[vertShader->id] = id;
    assetIdToResourceId[fragShader.id] = id;
    GLShader *shader{};
    switch (fragShader.materialType) {
    case MaterialType::Lit:
      shader = resourceRegistry.AddComponent<GLLitShader>(id);
      shader->type = MaterialType::Lit;
      break;
    default:
      shader = resourceRegistry.AddComponent<GLUnlitShader>(id);
      shader->type = MaterialType::Unlit;
      break;
    }
    const auto vertId = GLShaderBase::Compile(vertShader->text.data(), GL_VERTEX_SHADER, vertName);
    const auto fragId = GLShaderBase::Compile(fragShader.text.data(), GL_FRAGMENT_SHADER, name);
    vertShader->text = {};
    fragShader.text = {};
    const auto programId = glCreateProgram();
    shader->id = programId;
    glAttachShader(programId, vertId);
    glAttachShader(programId, fragId);
    glLinkProgram(programId);
    glDeleteShader(vertId);
    glDeleteShader(fragId);
    int success;
    glGetProgramiv(programId, GL_LINK_STATUS, &success);
    if (success) {
      shader->CacheLocations();
      spdlog::info("[GLRenderer] created shader: {}", name);
    } else {
      int logLength{};
      glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &logLength);
      std::string log(logLength, '\0');
      if (logLength > 0)
        glGetProgramInfoLog(programId, logLength, nullptr, log.data());
      spdlog::error("[GLRenderer] failed to create shader: {}\n{}", name, log);
    }
  }
}
template <>
inline auto GLRenderer::LoadAsset<TextureAsset>(TextureAsset &textureAsset) -> void {
  if (textureAsset.modelAssetId) {
    if (auto modelAsset = app.GetAsset<ModelAsset>(textureAsset.modelAssetId); modelAsset)
      LoadModelTexture(*modelAsset, textureAsset.modelTextureIndex);
    return;
  }
  if (resourceRegistry.IsEntity(GetAssetResourceId(textureAsset.id)))
    return;
  const auto name = app.GetAssetName(textureAsset.id);
  const auto id = resourceRegistry.Create(name);
  assetIdToResourceId[textureAsset.id] = id;
  auto glTexture = resourceRegistry.AddComponent<GLTexture>(id);
  *glTexture = LoadTexture(textureAsset.texture);
  if (textureAsset.texture.content == TextureContent::Skybox) {
    auto glSkybox = resourceRegistry.AddComponent<GLSkybox>(id);
    glSkybox->skybox = ConvertEquirectangularMapToCubemap({.format = TargetFormat::RGBA32, .type = TargetType::Cubemap, .width = 1024, .height = 1024, .mipmaps = 11}, glTexture->id, textureAsset.texture.flipY);
    glSkybox->irradiance = CreateIrradianceMap({.format = TargetFormat::RGBA32, .type = TargetType::Cubemap, .width = 64, .height = 64}, glSkybox->skybox, 1024u);
    glSkybox->prefilter = CreatePrefilterMap({.format = TargetFormat::RGBA32, .type = TargetType::Cubemap, .width = 256, .height = 256, .mipmaps = 7}, glSkybox->skybox);
    glSkybox->brdf = CreateBRDF_LUT({.format = TargetFormat::RG16, .width = 512, .height = 512});
  }
  spdlog::info("[GLRenderer] loaded texture: {}", name);
}
template <IsAsset T>
auto GLRenderer::PreviewAsset(T &) -> GLTexture * {
  return nullptr;
}
template <>
inline auto GLRenderer::PreviewAsset<MeshAsset>(MeshAsset &meshAsset) -> GLTexture * {
  if (const auto previewId = GetAssetPreviewId(meshAsset.id); resourceRegistry.IsEntity(previewId))
    return resourceRegistry.GetComponent<GLTexture>(previewId);
  auto shader = GetShader("Lit");
  if (!shader)
    return nullptr;
  const auto mesh = resourceRegistry.GetComponent<GLMesh>(GetAssetResourceId(meshAsset.id));
  if (!mesh)
    return nullptr;
  const auto materialAsset = app.GetAsset<MaterialAsset>(meshAsset.material);
  if (!materialAsset)
    return nullptr;
  const auto material = resourceRegistry.GetComponent<GLMaterial>(GetAssetResourceId(materialAsset->id));
  if (!material)
    return nullptr;
  const TargetDescription desc{.width = previewSize, .height = previewSize};
  const auto framebuffer = BorrowFramebuffer();
  const auto renderbuffer = BorrowRenderbuffer(desc);
  const auto cameraBuffer = BorrowBuffer(sizeof(CameraTransform));
  const auto materialBuffer = BorrowBuffer(sizeof(MaterialFallback));
  const auto transformBuffer = BorrowBuffer(sizeof(glm::mat4));
  const auto id = CreateTexture(desc);
  assetIdToPreviewId[meshAsset.id] = id;
  auto texture = GetTexture(id);
  Camera camera{};
  camera.Frame(meshAsset.bounds);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id, 0);
  glViewport(0, 0, previewSize, previewSize);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  shader->Use();
  shader->SetCamera(camera, cameraBuffer);
  shader->SetSkybox();
  Light light{};
  light.SetRotation(glm::quat(glm::vec3(.52f, .0f, .52f)));
  shader->SetLighting(light);
  shader->SetMaterial(*material);
  shader->SetMaterialFallback(*mesh, material->fallback, materialBuffer);
  shader->SetTransform(*mesh, glm::mat4(1.f), transformBuffer);
  shader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  ReturnBuffer(cameraBuffer, materialBuffer, transformBuffer);
  ReturnRenderbuffer(desc, renderbuffer);
  ReturnFramebuffer(framebuffer);
  spdlog::info("[GLRenderer] created preview for asset: {}", app.GetAssetName(meshAsset.id));
  return texture;
}
template <>
inline auto GLRenderer::PreviewAsset<ModelAsset>(ModelAsset &modelAsset) -> GLTexture * {
  if (const auto previewId = GetAssetPreviewId(modelAsset.id); resourceRegistry.IsEntity(previewId))
    return resourceRegistry.GetComponent<GLTexture>(previewId);
  auto shader = GetShader("Lit");
  if (!shader)
    return nullptr;
  if (modelAsset.nodes.size() == 0)
    return nullptr;
  const auto &rootNode = modelAsset.nodes[0];
  const TargetDescription desc{.width = previewSize, .height = previewSize};
  const auto framebuffer = BorrowFramebuffer();
  const auto renderbuffer = BorrowRenderbuffer(desc);
  const auto cameraBuffer = BorrowBuffer(sizeof(CameraTransform));
  const auto materialBuffer = BorrowBuffer(sizeof(MaterialFallback));
  const auto transformBuffer = BorrowBuffer(sizeof(glm::mat4));
  const auto id = CreateTexture(desc);
  assetIdToPreviewId[modelAsset.id] = id;
  auto texture = GetTexture(id);
  Camera camera{};
  camera.Frame(rootNode.bounds);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id, 0);
  glViewport(0, 0, previewSize, previewSize);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  shader->Use();
  shader->SetCamera(camera, cameraBuffer);
  shader->SetSkybox();
  Light light{};
  light.SetRotation(glm::quat(glm::vec3(.52f, .0f, .52f)));
  shader->SetLighting(light);
  std::vector<glm::mat4> transforms;
  transforms.reserve(modelAsset.nodes.size());
  for (const auto &node : modelAsset.nodes) {
    // NOTE: `nodes` array is created through depth-first traversal; in this loop, immediate parent transform will encode all other parents'
    auto parentTransform = glm::mat4(1.f);
    if (node.parent >= 0)
      // NOTE: node indices should correspond to transform indices — no upper bound check is needed
      parentTransform = transforms[node.parent];
    transforms.emplace_back(parentTransform * node.transform);
  }
  for (const auto &modelMesh : modelAsset.meshes) {
    const auto mesh = resourceRegistry.GetComponent<GLMesh>(modelMesh.resourceId);
    if (!mesh || mesh->skinned)
      continue;
    const auto &modelMaterial = modelAsset.materials[modelMesh.material];
    const auto material = resourceRegistry.GetComponent<GLMaterial>(modelMaterial.resourceId);
    if (!material)
      continue;
    const auto &transform = transforms[modelMesh.parent]; // NOTE: `mesh.parent` refers to a node; multiple meshes might share the same parent node
    shader->SetMaterial(*material);
    shader->SetMaterialFallback(*mesh, material->fallback, materialBuffer);
    shader->SetTransform(*mesh, transform, transformBuffer);
    shader->Draw(*mesh);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  ReturnBuffer(cameraBuffer, materialBuffer, transformBuffer);
  ReturnRenderbuffer(desc, renderbuffer);
  ReturnFramebuffer(framebuffer);
  spdlog::info("[GLRenderer] created preview for asset: {}", app.GetAssetName(modelAsset.id));
  return texture;
}
template <>
inline auto GLRenderer::PreviewAsset<TextureAsset>(TextureAsset &textureAsset) -> GLTexture * {
  if (textureAsset.modelAssetId) {
    if (auto modelAsset = app.GetAsset<ModelAsset>(textureAsset.modelAssetId); modelAsset)
      return LoadModelTexture(*modelAsset, textureAsset.modelTextureIndex);
    return nullptr;
  }
  return resourceRegistry.GetComponent<GLTexture>(GetAssetResourceId(textureAsset.id));
}
template <>
inline auto GLRenderer::PreviewAsset<MaterialAsset>(MaterialAsset &materialAsset) -> GLTexture * {
  if (const auto previewId = GetAssetPreviewId(materialAsset.id); resourceRegistry.IsEntity(previewId))
    return resourceRegistry.GetComponent<GLTexture>(previewId);
  auto shader = GetShader("Lit");
  if (!shader)
    return nullptr;
  const auto mesh = GetPrimitive("Sphere");
  if (!mesh)
    return nullptr;
  const auto material = resourceRegistry.GetComponent<GLMaterial>(GetAssetResourceId(materialAsset.id));
  if (!material)
    return nullptr;
  const BoundingBox previewBounds{.min = glm::vec3(-.5f), .max = glm::vec3(.5f)};
  const TargetDescription desc{.width = previewSize, .height = previewSize};
  const auto framebuffer = BorrowFramebuffer();
  const auto renderbuffer = BorrowRenderbuffer(desc);
  const auto cameraBuffer = BorrowBuffer(sizeof(CameraTransform));
  const auto materialBuffer = BorrowBuffer(sizeof(MaterialFallback));
  const auto transformBuffer = BorrowBuffer(sizeof(glm::mat4));
  const auto id = CreateTexture(desc);
  assetIdToPreviewId[materialAsset.id] = id;
  auto texture = GetTexture(id);
  Camera camera{};
  camera.Frame(previewBounds);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id, 0);
  glViewport(0, 0, previewSize, previewSize);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  shader->Use();
  shader->SetCamera(camera, cameraBuffer);
  shader->SetSkybox();
  Light light{};
  light.SetRotation(glm::quat(glm::vec3(.52f, .0f, .52f)));
  shader->SetLighting(light);
  shader->SetMaterial(*material);
  shader->SetMaterialFallback(*mesh, material->fallback, materialBuffer);
  shader->SetTransform(*mesh, glm::mat4(1.f), transformBuffer);
  shader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  ReturnBuffer(cameraBuffer, materialBuffer, transformBuffer);
  ReturnRenderbuffer(desc, renderbuffer);
  ReturnFramebuffer(framebuffer);
  spdlog::info("[GLRenderer] created preview for asset: {}", app.GetAssetName(materialAsset.id));
  return texture;
}
} // namespace kuki
