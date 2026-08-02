#pragma once
#include <array>
#include <asset_manager.hpp>
#include <bounding_box.hpp>
#include <buffer_pool.hpp>
#include <concepts.hpp>
#include <cstdint>
#include <framebuffer_pool.hpp>
#include <gl_buffer.hpp>
#include <gl_compute_shader.hpp>
#include <gl_framebuffer.hpp>
#include <gl_lit_shader.hpp>
#include <gl_mesh.hpp>
#include <gl_mesh_material.hpp>
#include <gl_render_target.hpp>
#include <gl_renderbuffer.hpp>
#include <gl_resource_registry.hpp>
#include <gl_shader.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <gl_unlit_shader.hpp>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <light.hpp>
#include <material_asset.hpp>
#include <material_fallback.hpp>
#include <mesh_asset.hpp>
#include <model_asset.hpp>
#include <render_graph.hpp>
#include <renderbuffer_pool.hpp>
#include <renderer.hpp>
#include <rendering_system.hpp>
#include <scene_manager.hpp>
#include <shader_type.hpp>
#include <target_description.hpp>
#include <texture_asset.hpp>
#include <texture_content.hpp>
#include <texture_pool.hpp>
#include <unordered_map>
#include <unordered_set>
namespace kuki {
struct GLFormat {
  int external;
  int internal;
};
inline constexpr int ASSET_PREVIEW_SIZE = 128;
inline constexpr unsigned int MAX_SPOT_SHADOW_LIGHTS = 8;
class KUKI_ENGINE_API GLRenderer final : public Renderer {
public:
  GLRenderer(Application &);
  static auto GLFormatToTarget(const unsigned int) -> TargetFormat;
  static auto TargetFormatToGL(const TargetFormat &) -> GLFormat;
  auto ApplyAntiAliasing(std::span<std::string>, std::span<std::string>) -> void override;
  auto ApplyBloomEffect(std::span<std::string>, std::span<std::string>) -> void override;
  auto ApplyBlurEffect(std::span<std::string>, std::span<std::string>) -> void override;
  auto ApplyBrightPassFilter(std::span<std::string>, std::span<std::string>) -> void override;
  auto ApplyGammaCorrection(std::span<std::string>, std::span<std::string>) -> void override;
  auto ApplyOutline(std::span<std::string>, std::span<std::string>) -> void override;
  auto BorrowBuffer(const int & = 0) -> unsigned int;
  auto BorrowFramebuffer() -> unsigned int;
  auto BorrowRenderbuffer(const TargetDescription &) -> unsigned int;
  auto BorrowTexture(const TargetDescription &) -> unsigned int;
  auto Clear() -> void override;
  auto CreateBuffer(const std::string & = "", const int & = 0) -> EntityID;
  auto CreateShadowMap(std::span<std::string>, std::span<std::string>) -> void override;
  auto CreateSpotShadowMap(std::span<std::string>, std::span<std::string>) -> void override;
  auto CreateTarget(const TargetDescription &, const std::string & = "") -> EntityID override;
  auto CreateTexture(const TargetDescription &, const std::string & = "") -> EntityID;
  auto DrawEntities(std::span<std::string>) -> void;
  auto DrawEntitiesInstanced(std::span<std::string>, const GLMesh &, const GLMaterial &, const std::vector<MaterialFallback> &, const std::vector<glm::mat4> &, const std::vector<uint32_t> &) -> void;
  auto DrawMeshes() -> void;
  auto DrawMeshesInstanced(const GLMesh &, const std::vector<glm::mat4> &, const glm::mat4 &view, const glm::mat4 &projection) -> void;
  auto DrawSkinnedEntities(std::span<std::string>) -> void;
  auto DrawSkybox() -> void;
  auto GetBuffer(const EntityID) -> GLBuffer *;
  auto GetBuffer(const std::string &) -> GLBuffer *;
  auto GetCompute(const std::string &) -> GLComputeShader *;
  auto GetPreviewSize() const -> int override;
  auto GetPrimitive(const std::string &) -> GLMesh *;
  auto GetResourceID(const std::string &) const -> EntityID;
  auto GetScene(this auto &, const std::string & = "") -> decltype(auto);
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
  auto PickEntity(const int, const int) -> EntityID override;
  auto PreviewAsset(const AssetID) -> GLTexture * override;
  auto RenderScene(std::span<std::string>, std::span<std::string>) -> void override;
  auto Reset() -> void override;
  auto SetPreviewSize(const int) -> void override;
  auto SetResolution(const int = 1920, const int = 1080) -> void override;
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
  BufferPool bufferPool;
  FramebufferPool framebufferPool;
  RenderbufferPool renderbufferPool;
  TexturePool texturePool;
  std::unordered_map<AssetID, EntityID> assetIdToResourceId;
  std::unordered_map<AssetID, EntityID> assetIdToPreviewId;
  std::unordered_map<GLMeshMat, std::vector<EntityID>> renderBuckets;
  std::unordered_set<EntityID> placeholderScaledMeshes;
  size_t renderBucketGeneration{static_cast<size_t>(-1)};
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
  static auto CreateIndexBuffer(GLMesh &, const std::vector<unsigned int> &) -> void;
  static auto CreateVertexBuffer(GLMesh &, const std::vector<Vertex> &, bool = false) -> void;
  auto ConvertCubemapToEquirectangularMap(const TargetDescription &, const unsigned int) -> unsigned int;
  auto ConvertEquirectangularMapToCubemap(const TargetDescription &, const unsigned int, bool = false) -> unsigned int;
  auto CreateBRDF_LUT(const TargetDescription &) -> unsigned int;
  auto CreateIrradianceMap(const TargetDescription &, const unsigned int, const unsigned int) -> unsigned int;
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
  auto RebuildRenderBuckets(Scene &) -> void;
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
