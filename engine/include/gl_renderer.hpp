#pragma once
#include <asset_manager.hpp>
#include <buffer_pool.hpp>
#include <component_manager.hpp>
#include <concepts.hpp>
#include <entity_manager.hpp>
#include <framebuffer_pool.hpp>
#include <gl_buffer.hpp>
#include <gl_compute_shader.hpp>
#include <gl_framebuffer.hpp>
#include <gl_lit_shader.hpp>
#include <gl_mesh.hpp>
#include <gl_render_target.hpp>
#include <gl_renderbuffer.hpp>
#include <gl_shader.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <gl_unlit_shader.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <material_asset.hpp>
#include <material_fallback.hpp>
#include <mesh_asset.hpp>
#include <render_graph.hpp>
#include <renderbuffer_pool.hpp>
#include <renderer.hpp>
#include <scene_asset.hpp>
#include <scene_manager.hpp>
#include <shader_type.hpp>
#include <target_description.hpp>
#include <texture_asset.hpp>
#include <texture_content.hpp>
#include <texture_pool.hpp>
//
#include <glad/glad.h>
namespace kuki {
struct GLFormat {
  int external;
  int internal;
};
class KUKI_ENGINE_API GLRenderer final : public Renderer {
public:
  GLRenderer(SceneManager &, AssetManager &);
  static auto GLFormatToTarget(const unsigned int) -> TargetFormat;
  static auto TargetFormatToGL(const TargetFormat &) -> GLFormat;
  auto BorrowBuffer(const int & = 0) -> unsigned int;
  auto BorrowFramebuffer() -> unsigned int;
  auto BorrowRenderbuffer(const TargetDescription &) -> unsigned int;
  auto BorrowTexture(const TargetDescription &) -> unsigned int;
  auto Clear() -> void override;
  auto CreateBuffer(const int & = 0, const std::string & = "") -> EntityID override;
  auto CreateBuffer(const std::string &, const int & = 0) -> EntityID override;
  auto CreateTarget(const TargetDescription &, const std::string & = "") -> EntityID override;
  auto CreateTarget(const std::string &, const TargetDescription &) -> EntityID override;
  auto CreateTexture(const TargetDescription &, const std::string & = "") -> EntityID override;
  auto CreateTexture(const std::string &, const TargetDescription &) -> EntityID override;
  auto GetBuffer(const EntityID) -> GLBuffer * override;
  auto GetBuffer(const std::string &) -> GLBuffer * override;
  auto GetCompute(const std::string &) -> GLComputeShader * override;
  auto GetPrimitive(const std::string &) -> GLMesh * override;
  auto GetResourceID(const std::string &) const -> EntityID;
  auto GetScene(const std::string & = "") -> Scene * override;
  auto GetShader(const MaterialType) -> GLShader * override;
  auto GetShader(const std::string &, const MaterialType = MaterialType::Unlit) -> GLShader * override;
  auto GetTarget(const EntityID) -> GLRenderTarget * override;
  auto GetTarget(const std::string &) -> GLRenderTarget * override;
  auto GetTexture(const EntityID) -> GLTexture * override;
  auto GetTexture(const std::string &) -> GLTexture * override;
  auto LoadAsset(const AssetID) -> void override;
  auto LoadScene(Scene &) -> void override;
  auto PreviewAsset(const AssetID) -> GLTexture * override;
  auto Reset() -> void override;
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
  AssetManager &assetManager;
  SceneManager &sceneManager;
  EntityManager resourceManager;
  BufferPool bufferPool;
  FramebufferPool framebufferPool;
  RenderbufferPool renderbufferPool;
  TexturePool texturePool;
  static auto CreateIndexBuffer(GLMesh &, const std::vector<unsigned int> &) -> void;
  static auto CreateVertexBuffer(GLMesh &, const std::vector<Vertex> &, bool = false) -> void;
  auto ConvertCubemapToEquirectangularMap(const TargetDescription &, const unsigned int) -> unsigned int;
  auto ConvertEquirectangularMapToCubemap(const TargetDescription &, const unsigned int) -> unsigned int;
  auto CreateBRDF_LUT(const TargetDescription &) -> unsigned int;
  auto CreateIrradianceMap(const TargetDescription &, const unsigned int) -> unsigned int;
  auto CreatePrefilterMap(const TargetDescription &, const unsigned int) -> unsigned int;
  auto LoadMesh(Mesh &) -> GLMesh;
  auto LoadSceneMaterial(SceneAsset &, const size_t) -> EntityID;
  auto LoadSceneMesh(SceneAsset &, const size_t) -> EntityID;
  auto LoadSceneTexture(SceneAsset &, const size_t) -> GLTexture *;
  auto LoadTexture(Texture &) -> GLTexture;
};
template <typename T>
auto GLRenderer::GetComponent(this auto &self, const EntityID id) -> decltype(auto) {
  return self.resourceManager.template GetComponent<T>(id);
}
template <typename T>
auto GLRenderer::GetComponent(this auto &self, const std::string &name) -> decltype(auto) {
  if (name.empty())
    return self.resourceManager.template GetAny<T>();
  return self.resourceManager.template GetComponent<T>(name);
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
  if (resourceManager.IsEntity(materialAsset.resourceId))
    return;
  const auto &name = materialAsset.GetName();
  const auto id = resourceManager.Create(name);
  materialAsset.resourceId = id;
  auto glMaterial = resourceManager.AddComponent<GLMaterial>(id);
  glMaterial->fallback = materialAsset.fallback;
  glMaterial->type = materialAsset.type;
  for (const auto &id : materialAsset.textures) {
    LoadAsset(id);
    const auto textureAsset = assetManager.Get<TextureAsset>(id);
    const auto texture = resourceManager.GetComponent<GLTexture>(textureAsset->resourceId);
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
  spdlog::info("[OpenGL] loaded material: {}", name);
}
template <>
inline auto GLRenderer::LoadAsset<MeshAsset>(MeshAsset &meshAsset) -> void {
  if (resourceManager.IsEntity(meshAsset.resourceId))
    return;
  const auto &name = meshAsset.GetName();
  const auto id = resourceManager.Create(name);
  meshAsset.resourceId = id;
  auto glMesh = resourceManager.AddComponent<GLMesh>(id);
  *glMesh = LoadMesh(meshAsset.mesh);
  LoadAsset(meshAsset.material);
  spdlog::info("[OpenGL] loaded mesh: {}", name);
}
template <>
inline auto GLRenderer::LoadAsset<SceneAsset>(SceneAsset &sceneAsset) -> void {
  // TODO: check if the asset is already loaded (individual materials and textures are already checked)
  // NOTE: materials and textures are loaded in the process if they are referenced by any mesh
  for (auto i = 0; i < sceneAsset.meshes.size(); ++i)
    LoadSceneMesh(sceneAsset, i);
}
template <>
inline auto GLRenderer::LoadAsset<ShaderAsset>(ShaderAsset &shaderAsset) -> void {
  if (resourceManager.IsEntity(shaderAsset.resourceId))
    return;
  const auto &name = shaderAsset.GetName();
  if (shaderAsset.shaderType == ShaderType::Compute) {
    auto &compShader = shaderAsset;
    const auto id = resourceManager.Create(name);
    compShader.resourceId = id;
    auto compute = resourceManager.AddComponent<GLComputeShader>(id);
    auto compId = GLShaderBase::Compile(compShader.text.data(), GL_COMPUTE_SHADER);
    const auto programId = glCreateProgram();
    compute->id = programId;
    glAttachShader(programId, compId);
    glLinkProgram(programId);
    glDeleteShader(compId);
    int success;
    glGetProgramiv(programId, GL_LINK_STATUS, &success);
    if (success) {
      compute->CacheLocations();
      spdlog::info("[OpenGL] created compute: {}", name);
    } else
      spdlog::error("[OpenGL] failed to create compute: {}", name);
  } else if (shaderAsset.shaderType == ShaderType::Fragment) {
    auto &fragShader = shaderAsset;
    auto vertAsset = assetManager.Get(fragShader.vertexShader);
    if (!vertAsset) {
      spdlog::error("Vertex shader for the following fragment shader could not be found: {}", name);
      return;
    }
    auto vertShader = vertAsset->As<ShaderAsset>();
    const auto &vertName = vertShader->GetName();
    if (!vertShader) {
      spdlog::error("Asset is not a shader: {}", vertName);
      return;
    }
    const auto id = resourceManager.Create(name);
    vertShader->resourceId = id;
    fragShader.resourceId = id;
    GLShader *shader{};
    switch (fragShader.materialType) {
    case MaterialType::Lit:
      shader = resourceManager.AddComponent<GLLitShader>(id);
      shader->type = MaterialType::Lit;
      break;
    default:
      shader = resourceManager.AddComponent<GLUnlitShader>(id);
      shader->type = MaterialType::Unlit;
      break;
    }
    const auto vertId = GLShaderBase::Compile(vertShader->text.data(), GL_VERTEX_SHADER, vertName);
    const auto fragId = GLShaderBase::Compile(fragShader.text.data(), GL_FRAGMENT_SHADER, name);
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
      spdlog::info("[OpenGL] created shader: {}", name);
    } else
      spdlog::error("[OpenGL] failed to create shader: {}", name);
  }
}
template <>
inline auto GLRenderer::LoadAsset<TextureAsset>(TextureAsset &textureAsset) -> void {
  if (resourceManager.IsEntity(textureAsset.resourceId))
    return;
  const auto &name = textureAsset.GetName();
  const auto id = resourceManager.Create(name);
  textureAsset.resourceId = id;
  auto glTexture = resourceManager.AddComponent<GLTexture>(id);
  *glTexture = LoadTexture(textureAsset.texture);
  if (textureAsset.texture.content == TextureContent::Skybox) {
    auto glSkybox = resourceManager.AddComponent<GLSkybox>(id);
    glSkybox->skybox = ConvertEquirectangularMapToCubemap({.format = TargetFormat::RGBA32, .type = TargetType::Cubemap, .width = 1024, .height = 1024}, glTexture->id);
    glSkybox->irradiance = CreateIrradianceMap({.format = TargetFormat::RGBA32, .type = TargetType::Cubemap, .width = 32, .height = 32}, glSkybox->skybox);
    glSkybox->prefilter = CreatePrefilterMap({.format = TargetFormat::RGBA32, .type = TargetType::Cubemap, .width = 128, .height = 128, .mipmaps = 7}, glSkybox->skybox);
    glSkybox->brdf = CreateBRDF_LUT({.format = TargetFormat::RG16, .width = 512, .height = 512});
  }
  spdlog::info("[OpenGL] loaded texture: {}", name);
}
template <IsAsset T>
auto GLRenderer::PreviewAsset(T &) -> GLTexture * {
  return nullptr;
}
template <>
inline auto GLRenderer::PreviewAsset<MeshAsset>(MeshAsset &meshAsset) -> GLTexture * {
  constexpr unsigned int PREVIEW_SIZE = 128; // TODO: make this configurable
  if (meshAsset.previewId)
    return resourceManager.GetComponent<GLTexture>(meshAsset.previewId);
  auto shader = GetShader(MaterialType::Lit);
  if (!shader)
    return nullptr;
  const auto mesh = resourceManager.GetComponent<GLMesh>(meshAsset.resourceId);
  if (!mesh)
    return nullptr;
  const auto materialAsset = assetManager.Get<MaterialAsset>(meshAsset.material);
  if (!materialAsset)
    return nullptr;
  const auto material = resourceManager.GetComponent<GLMaterial>(materialAsset->resourceId);
  if (!material)
    return nullptr;
  const TargetDescription desc{.width = PREVIEW_SIZE, .height = PREVIEW_SIZE};
  const auto framebuffer = BorrowFramebuffer();
  const auto renderbuffer = BorrowRenderbuffer(desc);
  const auto cameraBuffer = BorrowBuffer(sizeof(CameraTransform));
  const auto materialBuffer = BorrowBuffer(sizeof(MaterialFallback));
  const auto transformBuffer = BorrowBuffer(sizeof(glm::mat4));
  const auto id = CreateTexture(desc);
  meshAsset.previewId = id;
  auto texture = GetTexture(id);
  Camera camera{};
  camera.Frame(meshAsset.bounds);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id, 0);
  glViewport(0, 0, PREVIEW_SIZE, PREVIEW_SIZE);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  shader->Use();
  shader->SetCamera(camera, cameraBuffer);
  shader->SetSkybox();
  shader->SetUniform("hasSkybox", true);
  shader->SetLighting();
  shader->SetMaterial(*material);
  shader->SetMaterialFallback(*mesh, material->fallback, materialBuffer);
  shader->SetTransform(*mesh, glm::mat4(1.f), transformBuffer);
  shader->Draw(*mesh);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  ReturnBuffer(cameraBuffer, materialBuffer, transformBuffer);
  ReturnRenderbuffer(desc, renderbuffer);
  ReturnFramebuffer(framebuffer);
  spdlog::info("[OpenGL] created preview for asset: {}", meshAsset.GetName());
  return texture;
}
template <>
inline auto GLRenderer::PreviewAsset<SceneAsset>(SceneAsset &sceneAsset) -> GLTexture * {
  constexpr unsigned int PREVIEW_SIZE = 128; // TODO: make this configurable
  if (sceneAsset.previewId)
    return resourceManager.GetComponent<GLTexture>(sceneAsset.previewId);
  auto shader = GetShader(MaterialType::Lit);
  if (!shader)
    return nullptr;
  if (sceneAsset.nodes.size() == 0)
    return nullptr;
  const auto &rootNode = sceneAsset.nodes[0];
  const TargetDescription desc{.width = PREVIEW_SIZE, .height = PREVIEW_SIZE};
  const auto framebuffer = BorrowFramebuffer();
  const auto renderbuffer = BorrowRenderbuffer(desc);
  const auto cameraBuffer = BorrowBuffer(sizeof(CameraTransform));
  const auto materialBuffer = BorrowBuffer(sizeof(MaterialFallback));
  const auto transformBuffer = BorrowBuffer(sizeof(glm::mat4));
  const auto id = CreateTexture(desc);
  sceneAsset.previewId = id;
  auto texture = GetTexture(id);
  Camera camera{};
  camera.Frame(rootNode.bounds);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id, 0);
  glViewport(0, 0, PREVIEW_SIZE, PREVIEW_SIZE);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  shader->Use();
  shader->SetCamera(camera, cameraBuffer);
  shader->SetSkybox();
  shader->SetLighting();
  std::vector<glm::mat4> transforms;
  transforms.reserve(sceneAsset.nodes.size());
  for (const auto &node : sceneAsset.nodes) {
    // NOTE: `nodes` array is created through depth-first traversal; in this loop, immediate parent transform will encode all other parents'
    auto parentTransform = glm::mat4(1.f);
    if (node.parent >= 0)
      // NOTE: node indices should correspond to transform indices — no upper bound check is needed
      parentTransform = transforms[node.parent];
    transforms.emplace_back(parentTransform * node.transform);
  }
  for (const auto &sceneMesh : sceneAsset.meshes) {
    const auto mesh = resourceManager.GetComponent<GLMesh>(sceneMesh.resourceId);
    if (!mesh)
      continue;
    const auto &sceneMaterial = sceneAsset.materials[sceneMesh.material];
    const auto material = resourceManager.GetComponent<GLMaterial>(sceneMaterial.resourceId);
    if (!material)
      continue;
    const auto &transform = transforms[sceneMesh.parent]; // NOTE: `mesh.parent` refers to a node; multiple meshes might share the same parent node
    shader->SetMaterial(*material);
    shader->SetMaterialFallback(*mesh, material->fallback, materialBuffer);
    shader->SetTransform(*mesh, transform, transformBuffer);
    shader->Draw(*mesh);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  ReturnBuffer(cameraBuffer, materialBuffer, transformBuffer);
  ReturnRenderbuffer(desc, renderbuffer);
  ReturnFramebuffer(framebuffer);
  spdlog::info("[OpenGL] created preview for asset: {}", sceneAsset.GetName());
  return texture;
}
} // namespace kuki
