#include <asset_manager.hpp>
#include <buffer_object.hpp>
#include <gl_buffer.hpp>
#include <gl_compute_shader.hpp>
#include <gl_mesh_material.hpp>
#include <gl_render_target.hpp>
#include <gl_renderer.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <id.hpp>
#include <material_handle.hpp>
#include <material_type.hpp>
#include <mesh_handle.hpp>
#include <primitive.hpp>
#include <render_graph.hpp>
#include <render_target.hpp>
#include <renderer.hpp>
#include <scene.hpp>
#include <scene_asset.hpp>
#include <skybox_handle.hpp>
#include <target_description.hpp>
#include <texture_content.hpp>
//
#include <glad/glad.h>
namespace kuki {
GLRenderer::GLRenderer(SceneManager &sceneManager, AssetManager &assetManager)
  : Renderer(std::in_place_type<GLRenderer>), sceneManager(sceneManager), assetManager(assetManager) {}
auto GLRenderer::BorrowBuffer(const int &size) -> unsigned int {
  return bufferPool.Request(size);
}
auto GLRenderer::BorrowFramebuffer() -> unsigned int {
  return framebufferPool.Request();
}
auto GLRenderer::BorrowRenderbuffer(const TargetDescription &desc) -> unsigned int {
  return renderbufferPool.Request(desc);
}
auto GLRenderer::BorrowTexture(const TargetDescription &desc) -> unsigned int {
  return texturePool.Request(desc);
}
auto GLRenderer::Clear() -> void {
  resourceManager.Clear();
}
auto GLRenderer::CreateBuffer(const std::string &name, const int &size) -> EntityID {
  EntityID id{};
  if (auto id_ = resourceManager.GetID(name); id_)
    id = id_;
  else
    id = resourceManager.Create(name);
  auto buffer = resourceManager.AddComponent<GLBuffer>(id);
  if (buffer->id == 0) {
    buffer->id = BorrowBuffer(size);
    spdlog::info("[OpenGL] created buffer: {}", name);
  }
  return id;
}
auto GLRenderer::CreateTarget(const TargetDescription &desc, const std::string &name) -> EntityID {
  EntityID id{};
  if (auto id_ = resourceManager.GetID(name); id_)
    id = id_;
  else
    id = resourceManager.Create(name);
  auto renderTarget = resourceManager.AddComponent<GLRenderTarget>(id);
  if (renderTarget->framebuffer == 0) {
    renderTarget->framebuffer = BorrowFramebuffer();
    spdlog::info("[OpenGL] created render target: {}", name);
  }
  if (renderTarget->renderbuffer == 0)
    renderTarget->renderbuffer = BorrowRenderbuffer(desc);
  if (renderTarget->texture == 0)
    renderTarget->texture = BorrowTexture(desc);
  renderTarget->desc = desc;
  glBindFramebuffer(GL_FRAMEBUFFER, renderTarget->framebuffer);
  const auto textureTarget = desc.samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
  const auto attachment = desc.format == TargetFormat::DEPTH ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0;
  glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, textureTarget, renderTarget->texture, 0);
  if (attachment != GL_DEPTH_ATTACHMENT)
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderTarget->renderbuffer);
  else {
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return id;
}
auto GLRenderer::CreateTexture(const TargetDescription &desc, const std::string &name) -> EntityID {
  EntityID id{};
  if (auto id_ = resourceManager.GetID(name); id_)
    id = id_;
  else
    id = resourceManager.Create(name);
  auto texture = resourceManager.AddComponent<GLTexture>(id);
  if (texture->id == 0) {
    texture->id = BorrowTexture(desc);
    spdlog::info("[OpenGL] created texture: {}", name);
  }
  return id;
}
auto GLRenderer::GetBuffer(const EntityID id) -> GLBuffer * {
  return resourceManager.GetComponent<GLBuffer>(id);
}
auto GLRenderer::GetBuffer(const std::string &name) -> GLBuffer * {
  return resourceManager.GetComponent<GLBuffer>(name);
}
auto GLRenderer::GetCompute(const std::string &name) -> GLComputeShader * {
  return resourceManager.GetComponent<GLComputeShader>(name);
}
auto GLRenderer::GetPrimitive(const std::string &name) -> GLMesh * {
  return resourceManager.GetComponent<GLMesh>(name);
}
auto GLRenderer::GetResourceID(const std::string &name) const -> EntityID {
  return resourceManager.GetID(name);
}
auto GLRenderer::GetScene(const std::string &name) -> Scene * {
  if (name.empty())
    return sceneManager.GetActive();
  return sceneManager.Get(name);
}
auto GLRenderer::GetShader(const MaterialType type) -> GLShader * {
  switch (type) {
  case MaterialType::Lit:
    return resourceManager.GetAny<GLLitShader>();
  default:
    return resourceManager.GetAny<GLUnlitShader>();
  }
}
auto GLRenderer::GetShader(const std::string &name, const MaterialType type) -> GLShader * {
  switch (type) {
  case MaterialType::Lit:
    return resourceManager.GetComponent<GLLitShader>(name);
  default:
    return resourceManager.GetComponent<GLUnlitShader>(name);
  }
}
auto GLRenderer::GetTarget(const EntityID id) -> GLRenderTarget * {
  return resourceManager.GetComponent<GLRenderTarget>(id);
}
auto GLRenderer::GetTarget(const std::string &name) -> GLRenderTarget * {
  return resourceManager.GetComponent<GLRenderTarget>(name);
}
auto GLRenderer::GetTexture(const EntityID id) -> GLTexture * {
  return resourceManager.GetComponent<GLTexture>(id);
}
auto GLRenderer::GetTexture(const std::string &name) -> GLTexture * {
  return resourceManager.GetComponent<GLTexture>(name);
}
auto GLRenderer::LoadAsset(const AssetID id) -> void {
  auto asset = assetManager.Get(id);
  if (!asset)
    return;
  if (asset->Is<MaterialAsset>())
    LoadAsset<MaterialAsset>(*asset->As<MaterialAsset>());
  else if (asset->Is<MeshAsset>())
    LoadAsset<MeshAsset>(*asset->As<MeshAsset>());
  else if (asset->Is<SceneAsset>())
    LoadAsset<SceneAsset>(*asset->As<SceneAsset>());
  else if (asset->Is<ShaderAsset>())
    LoadAsset<ShaderAsset>(*asset->As<ShaderAsset>());
  else if (asset->Is<TextureAsset>())
    LoadAsset<TextureAsset>(*asset->As<TextureAsset>());
}
auto GLRenderer::LoadScene(Scene &scene) -> void {
  scene.ForEachEntity<SceneMeshHandle, SceneMaterialHandle>([&](const EntityID id, SceneMeshHandle *meshHandle, SceneMaterialHandle *materialHandle) {
    if (meshHandle->resourceId && materialHandle->resourceId)
      return;
    auto sceneAsset = assetManager.Get<SceneAsset>(meshHandle->sceneAssetId);
    if (!sceneAsset)
      return;
    if (!meshHandle->resourceId)
      meshHandle->resourceId = LoadSceneMesh(*sceneAsset, meshHandle->meshIndex);
    if (!materialHandle->resourceId)
      materialHandle->resourceId = LoadSceneMaterial(*sceneAsset, materialHandle->materialIndex);
    auto [mesh, material] = scene.AddEntityComponent<GLMesh, GLMaterial>(id);
    if (!mesh || !material)
      return;
    auto mesh_ = resourceManager.GetComponent<GLMesh>(meshHandle->resourceId);
    auto material_ = resourceManager.GetComponent<GLMaterial>(materialHandle->resourceId);
    if (!mesh_ || !material_)
      return;
    *mesh = *mesh_;
    *material = *material_;
  });
  scene.ForEachEntity<MeshHandle, MaterialHandle>([&](const EntityID id, MeshHandle *meshHandle, MaterialHandle *materialHandle) {
    if (meshHandle->resourceId && materialHandle->resourceId)
      return;
    auto meshAsset = assetManager.Get<MeshAsset>(meshHandle->assetId);
    auto materialAsset = assetManager.Get<MaterialAsset>(materialHandle->assetId);
    if (!meshAsset || !materialAsset)
      return;
    if (!meshHandle->resourceId) {
      LoadAsset<MeshAsset>(*meshAsset);
      meshHandle->resourceId = meshAsset->resourceId;
    }
    if (!materialHandle->resourceId) {
      LoadAsset<MaterialAsset>(*materialAsset);
      materialHandle->resourceId = materialAsset->resourceId;
    }
    auto [mesh, material] = scene.AddEntityComponent<GLMesh, GLMaterial>(id);
    if (!mesh || !material)
      return;
    auto mesh_ = resourceManager.GetComponent<GLMesh>(meshHandle->resourceId);
    auto material_ = resourceManager.GetComponent<GLMaterial>(materialHandle->resourceId);
    if (!mesh_ || !material_)
      return;
    *mesh = *mesh_;
    *material = *material_;
  });
}
auto GLRenderer::PreviewAsset(const AssetID id) -> GLTexture * {
  if (!assetManager.IsRegistered(id))
    return nullptr;
  LoadAsset(id);
  auto asset = assetManager.Get(id);
  if (asset->Is<MeshAsset>())
    return PreviewAsset<MeshAsset>(*asset->As<MeshAsset>());
  else if (asset->Is<SceneAsset>())
    return PreviewAsset<SceneAsset>(*asset->As<SceneAsset>());
  return nullptr;
}
auto GLRenderer::Reset() -> void {
  glClearColor(0.f, 0.f, 0.f, 0.f);
}
auto GLRenderer::UpdateTarget(const std::string &name, const TargetDescription &desc) -> void {
  const auto id = resourceManager.GetID(name);
  if (!id)
    return;
  auto target = resourceManager.GetComponent<GLRenderTarget>(id);
  if (!target)
    return;
  target->desc = desc;
  renderbufferPool.Reallocate(desc, target->renderbuffer);
  texturePool.Reallocate(desc, target->texture);
  glBindFramebuffer(GL_FRAMEBUFFER, target->framebuffer);
  const auto textureTarget = desc.samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
  const auto attachment = desc.format == TargetFormat::DEPTH ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0;
  glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, textureTarget, target->texture, 0);
  if (attachment != GL_DEPTH_ATTACHMENT)
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, target->renderbuffer);
  else {
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
auto GLRenderer::CreateIndexBuffer(GLMesh &mesh, const std::vector<unsigned int> &indices) -> void {
  mesh.indexCount = indices.size();
  GLuint indexBuffer;
  glCreateBuffers(1, &indexBuffer);
  mesh.ebo = indexBuffer;
  glNamedBufferData(mesh.ebo, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
  glVertexArrayElementBuffer(mesh.vao, mesh.ebo);
}
auto GLRenderer::CreateVertexBuffer(GLMesh &mesh, const std::vector<Vertex> &vertices, bool skinned) -> void {
  mesh.vertexCount = vertices.size();
  GLuint vao, vbo;
  glCreateVertexArrays(1, &vao);
  glCreateBuffers(1, &vbo);
  mesh.vao = vao;
  auto bindingIndex = 0;
  glNamedBufferData(vbo, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
  glVertexArrayVertexBuffer(mesh.vao, bindingIndex, vbo, 0, sizeof(Vertex));
  auto attribIndex = 0;
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, texture));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, tangent));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  if (skinned) {
    attribIndex++;
    glVertexArrayAttribFormat(mesh.vao, attribIndex, 4, GL_INT, GL_FALSE, offsetof(Vertex, boneIds));
    glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
    glEnableVertexArrayAttrib(mesh.vao, attribIndex);
    attribIndex++;
    glVertexArrayAttribFormat(mesh.vao, attribIndex, 4, GL_FLOAT, GL_FALSE, offsetof(Vertex, boneWeights));
    glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
    glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  }
}
auto GLRenderer::ConvertCubemapToEquirectangularMap(const TargetDescription &desc, const unsigned int input) -> unsigned int {
  auto compute = GetCompute("CubemapEquirect");
  if (!compute)
    return 0;
  const auto output = BorrowTexture(desc);
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  compute->SetTexture("cubemap", input);
  compute->SetUniform("size", static_cast<unsigned int>(desc.width));
  const auto format = TargetFormatToGL(desc.format);
  glBindImageTexture(0, output, 0, GL_TRUE, 0, GL_WRITE_ONLY, format.internal);
  const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
  const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
  compute->Dispatch(numGroupsX, numGroupsY, 6);
  return output;
}
auto GLRenderer::ConvertEquirectangularMapToCubemap(const TargetDescription &desc, const unsigned int input) -> unsigned int {
  auto compute = GetCompute("EquirectCubemap");
  if (!compute)
    return 0;
  const auto output = BorrowTexture(desc);
  const auto format = TargetFormatToGL(desc.format);
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  compute->SetTexture("equirect", input);
  compute->SetUniform("size", static_cast<unsigned int>(desc.width));
  // TODO: set the following to `true` if texture was loaded by TinyEXR
  compute->SetUniform("invert", false);
  glBindImageTexture(0, output, 0, GL_TRUE, 0, GL_WRITE_ONLY, format.internal);
  const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
  const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
  compute->Dispatch(numGroupsX, numGroupsY, 6);
  return output;
}
auto GLRenderer::CreateBRDF_LUT(const TargetDescription &desc) -> unsigned int {
  auto compute = GetCompute("BRDF_LUT");
  if (!compute)
    return 0;
  if (const auto output = GetTexture("BRDF_LUT"); output)
    return output->id;
  const auto id = CreateTexture(desc, "BRDF_LUT");
  const auto output = GetTexture(id);
  const auto format = TargetFormatToGL(desc.format);
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  glBindImageTexture(0, output->id, 0, GL_FALSE, 0, GL_WRITE_ONLY, format.internal);
  const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
  const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
  compute->Dispatch(numGroupsX, numGroupsY, 1);
  return output->id;
}
auto GLRenderer::CreateIrradianceMap(const TargetDescription &desc, const unsigned int input) -> unsigned int {
  auto compute = GetCompute("IrradianceMap");
  if (!compute)
    return 0;
  const auto output = BorrowTexture(desc);
  const auto format = TargetFormatToGL(desc.format);
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  compute->SetTexture("cubemap", input);
  compute->SetUniform("cubeSize", static_cast<unsigned int>(desc.width));
  glBindImageTexture(0, output, 0, GL_TRUE, 0, GL_WRITE_ONLY, format.internal);
  const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
  const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
  compute->Dispatch(numGroupsX, numGroupsY, 6);
  return output;
}
auto GLRenderer::CreatePrefilterMap(const TargetDescription &desc, const unsigned int input) -> unsigned int {
  auto compute = GetCompute("PrefilterMap");
  if (!compute)
    return 0;
  const auto output = BorrowTexture(desc);
  const auto format = TargetFormatToGL(desc.format);
  constexpr unsigned int workgroupSize = 8;
  compute->Use();
  compute->SetTexture("cubemap", input);
  compute->SetUniform("mipLevels", desc.mipmaps);
  for (auto mip = 0; mip < desc.mipmaps; ++mip) {
    const auto mipSize = static_cast<unsigned int>(desc.width) >> mip;
    const auto roughness = static_cast<float>(mip) / (desc.mipmaps - 1);
    compute->SetUniform("roughness", roughness);
    compute->SetUniform("mipWidth", mipSize);
    compute->SetUniform("cubeSize", mipSize);
    glBindImageTexture(0, output, mip, GL_TRUE, 0, GL_WRITE_ONLY, format.internal);
    const auto numGroupsX = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.width) / workgroupSize));
    const auto numGroupsY = static_cast<unsigned int>(std::ceil(static_cast<float>(desc.height) / workgroupSize));
    compute->Dispatch(numGroupsX, numGroupsY, 6);
  }
  return output;
}
auto GLRenderer::LoadMesh(Mesh &mesh) -> GLMesh {
  GLMesh glMesh;
  CreateVertexBuffer(glMesh, mesh.vertices);
  if (mesh.indices.size() > 0)
    CreateIndexBuffer(glMesh, mesh.indices);
  mesh.vertices = {};
  mesh.indices = {};
  return glMesh;
}
auto GLRenderer::LoadTexture(Texture &texture) -> GLTexture {
  GLTexture glTexture;
  glTexture.desc.width = texture.width;
  glTexture.desc.height = texture.height;
  glTexture.content = texture.content;
  const auto isHDR = texture.range == ColorRange::HDR;
  const auto isSRGB = texture.color == ColorSpace::sRGB;
  GLenum internalFormat, format;
  switch (texture.channels) {
  case 1:
    internalFormat = GL_R8;
    format = GL_RED;
    break;
  case 2:
    internalFormat = GL_RG8;
    format = GL_RG;
    break;
  case 3:
    if (isHDR)
      internalFormat = GL_RGB16F;
    else if (isSRGB)
      internalFormat = GL_SRGB8;
    else
      internalFormat = GL_RGB8;
    format = GL_RGB;
    break;
  case 4:
    if (isHDR)
      internalFormat = GL_RGBA16F;
    else if (isSRGB)
      internalFormat = GL_SRGB8_ALPHA8;
    else
      internalFormat = GL_RGBA8;
    format = GL_RGBA;
    break;
  default:
    spdlog::warn("[OpenGL] unsupported number of channels: {}", texture.channels);
    return glTexture;
  }
  glTexture.desc.format = GLFormatToTarget(internalFormat);
  glCreateTextures(GL_TEXTURE_2D, 1, &glTexture.id);
  auto mipmaps = 1;
  if (isSRGB)
    mipmaps = std::log2(std::max(texture.width, texture.height)) + 1;
  glTextureStorage2D(glTexture.id, mipmaps, internalFormat, texture.width, texture.height);
  const auto type = isHDR ? GL_FLOAT : GL_UNSIGNED_BYTE;
  std::visit([&](auto &data) { glTextureSubImage2D(glTexture.id, 0, 0, 0, texture.width, texture.height, format, type, data.data()); }, texture.data);
  switch (texture.content) {
  case TextureContent::Skybox:
    glTextureParameteri(glTexture.id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(glTexture.id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(glTexture.id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    break;
  default:
    glTextureParameteri(glTexture.id, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(glTexture.id, GL_TEXTURE_WRAP_T, GL_REPEAT);
    if (texture.channels == 1) {
      glTextureParameteri(glTexture.id, GL_TEXTURE_SWIZZLE_G, GL_RED);
      glTextureParameteri(glTexture.id, GL_TEXTURE_SWIZZLE_B, GL_RED);
      glTextureParameteri(glTexture.id, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    }
    glTextureParameteri(glTexture.id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateTextureMipmap(glTexture.id);
    break;
  }
  texture.data = {};
  return glTexture;
}
auto GLRenderer::LoadSceneMaterial(SceneAsset &sceneAsset, const size_t materialIndex) -> EntityID {
  if (materialIndex >= sceneAsset.materials.size())
    return EntityID::Invalid;
  auto &sceneMaterial = sceneAsset.materials[materialIndex];
  if (sceneMaterial.resourceId)
    return sceneMaterial.resourceId;
  const auto resourceId = resourceManager.Create(sceneMaterial.name);
  sceneMaterial.resourceId = resourceId;
  auto material = resourceManager.AddComponent<GLMaterial>(resourceId);
  material->fallback = sceneMaterial.fallback;
  material->type = sceneMaterial.type;
  for (auto i = 0; i < sceneMaterial.textures.size(); ++i) {
    const auto textureIndex = sceneMaterial.textures[i];
    const auto texture = LoadSceneTexture(sceneAsset, textureIndex);
    if (!texture)
      continue;
    switch (texture->content) {
    case TextureContent::Emissive:
      material->textures.emissive = texture->id;
      material->fallback.textureMask.set(static_cast<int>(TextureContent::Emissive));
      break;
    case TextureContent::Metalness:
      material->textures.metalness = texture->id;
      material->fallback.textureMask.set(static_cast<int>(TextureContent::Metalness));
      break;
    case TextureContent::Normal:
      material->textures.normal = texture->id;
      material->fallback.textureMask.set(static_cast<int>(TextureContent::Normal));
      break;
    case TextureContent::Occlusion:
      material->textures.occlusion = texture->id;
      material->fallback.textureMask.set(static_cast<int>(TextureContent::Occlusion));
      break;
    case TextureContent::Roughness:
      material->textures.roughness = texture->id;
      material->fallback.textureMask.set(static_cast<int>(TextureContent::Roughness));
      break;
    case TextureContent::Specular:
      material->textures.specular = texture->id;
      material->fallback.textureMask.set(static_cast<int>(TextureContent::Specular));
      break;
    default:
      material->textures.albedo = texture->id;
      material->fallback.textureMask.set(static_cast<int>(TextureContent::Albedo));
      break;
    }
  }
  spdlog::info("[OpenGL] loaded material: {}", sceneMaterial.name);
  return resourceId;
}
auto GLRenderer::LoadSceneMesh(SceneAsset &sceneAsset, const size_t meshIndex) -> EntityID {
  if (meshIndex >= sceneAsset.meshes.size())
    return EntityID::Invalid;
  auto &sceneMesh = sceneAsset.meshes[meshIndex];
  if (sceneMesh.resourceId)
    return sceneMesh.resourceId;
  const auto resourceId = resourceManager.Create(sceneMesh.name);
  sceneMesh.resourceId = resourceId;
  auto glMesh = resourceManager.AddComponent<GLMesh>(resourceId);
  *glMesh = LoadMesh(sceneMesh.mesh);
  const auto materialIndex = sceneMesh.material;
  LoadSceneMaterial(sceneAsset, materialIndex);
  spdlog::info("[OpenGL] loaded mesh: {}", sceneMesh.name);
  return resourceId;
}
auto GLRenderer::LoadSceneTexture(SceneAsset &sceneAsset, const size_t textureIndex) -> GLTexture * {
  if (textureIndex >= sceneAsset.textures.size())
    return nullptr;
  auto &sceneTexture = sceneAsset.textures[textureIndex];
  if (sceneTexture.resourceId)
    return resourceManager.GetComponent<GLTexture>(sceneTexture.resourceId);
  const auto resourceId = resourceManager.Create(sceneTexture.name);
  sceneTexture.resourceId = resourceId;
  auto glTexture = resourceManager.AddComponent<GLTexture>(resourceId);
  *glTexture = LoadTexture(sceneTexture.texture);
  spdlog::info("[OpenGL] loaded texture: {}", sceneTexture.name);
  return glTexture;
}
auto GLRenderer::GLFormatToTarget(const unsigned int format) -> TargetFormat {
  switch (format) {
  case GL_R8:
    return TargetFormat::R8;
  case GL_RG8:
    return TargetFormat::RG8;
  case GL_RGB8:
    return TargetFormat::RGB8;
  case GL_R16F:
    return TargetFormat::R16;
  case GL_RG16F:
    return TargetFormat::RG16;
  case GL_RGB16F:
    return TargetFormat::RGB16;
  case GL_RGB32F:
    return TargetFormat::RGB32;
  case GL_RGBA8:
    return TargetFormat::RGBA8;
  case GL_RGBA16F:
    return TargetFormat::RGBA16;
  case GL_RGBA32F:
    return TargetFormat::RGBA32;
  case GL_DEPTH_COMPONENT:
    return TargetFormat::DEPTH;
  default:
    return TargetFormat::Unknown;
  }
}
auto GLRenderer::TargetFormatToGL(const TargetFormat &format) -> GLFormat {
  switch (format) {
  case TargetFormat::R8:
    return {GL_R, GL_R8};
  case TargetFormat::RG8:
    return {GL_RG, GL_RG8};
  case TargetFormat::RGB8:
    return {GL_RGB, GL_RGB8};
  case TargetFormat::R16:
    return {GL_R, GL_R16F};
  case TargetFormat::RG16:
    return {GL_RG, GL_RG16F};
  case TargetFormat::RGB16:
    return {GL_RGB, GL_RGB16F};
  case TargetFormat::RGB32:
    return {GL_RGB, GL_RGB32F};
  case TargetFormat::RGBA8:
    return {GL_RGBA, GL_RGBA8};
  case TargetFormat::RGBA16:
    return {GL_RGBA, GL_RGBA16F};
  case TargetFormat::RGBA32:
    return {GL_RGBA, GL_RGBA32F};
  case TargetFormat::DEPTH:
    return {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT};
  default:
    return {};
  }
}
} // namespace kuki
