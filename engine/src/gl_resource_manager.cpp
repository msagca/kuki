#include <asset_manager.hpp>
#include <buffer_description.hpp>
#include <color_space.hpp>
#include <gl_buffer.hpp>
#include <gl_compute_shader.hpp>
#include <gl_framebuffer.hpp>
#include <gl_material.hpp>
#include <gl_render_target.hpp>
#include <gl_resource_manager.hpp>
#include <gl_shader.hpp>
#include <gl_texture.hpp>
#include <id.hpp>
#include <scene_asset.hpp>
#include <spdlog/spdlog.h>
#include <target_description.hpp>
#include <texture_content.hpp>
//
#include <glad/glad.h>
namespace kuki {
GLResourceManager::GLResourceManager(AssetManager &assetManager)
  : assetManager(assetManager) {}
auto GLResourceManager::GLFormatToTarget(const unsigned int format) -> TargetFormat {
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
  case GL_SRGB8:
    return TargetFormat::SRGB8;
  default:
    return TargetFormat::Unknown;
  }
}
auto GLResourceManager::GLTypeToTarget(const unsigned int target) -> TargetType {
  switch (target) {
  case GL_TEXTURE_2D:
    return TargetType::Texture2D;
  case GL_TEXTURE_2D_MULTISAMPLE:
    return TargetType::Texture2DMulti;
  case GL_TEXTURE_CUBE_MAP:
    return TargetType::Cubemap;
  default:
    return TargetType::Unknown;
  }
}
auto GLResourceManager::TargetFormatToGL(const TargetFormat &format) -> unsigned int {
  switch (format) {
  case TargetFormat::R8:
    return GL_R8;
  case TargetFormat::RG8:
    return GL_RG8;
  case TargetFormat::RGB8:
    return GL_RGB8;
  case TargetFormat::R16:
    return GL_R16F;
  case TargetFormat::RG16:
    return GL_RG16F;
  case TargetFormat::RGB16:
    return GL_RGB16F;
  case TargetFormat::RGB32:
    return GL_RGB32F;
  case TargetFormat::RGBA8:
    return GL_RGBA8;
  case TargetFormat::RGBA16:
    return GL_RGBA16F;
  case TargetFormat::RGBA32:
    return GL_RGBA32F;
  case TargetFormat::SRGB8:
    return GL_SRGB8;
  default:
    return 0;
  }
}
auto GLResourceManager::TargetTypeToGL(const TargetType &type) -> unsigned int {
  switch (type) {
  case TargetType::Texture2D:
    return GL_TEXTURE_2D;
  case TargetType::Texture2DMulti:
    return GL_TEXTURE_2D_MULTISAMPLE;
  case TargetType::Cubemap:
    return GL_TEXTURE_CUBE_MAP;
  default:
    return 0;
  }
}
auto GLResourceManager::BorrowBuffer(const BufferDescription &desc) -> unsigned int {
  return bufferPool.Request(desc);
}
auto GLResourceManager::BorrowFramebuffer() -> unsigned int {
  return framebufferPool.Request();
}
auto GLResourceManager::BorrowRenderbuffer(const TargetDescription &desc) -> unsigned int {
  return renderbufferPool.Request(desc);
}
auto GLResourceManager::BorrowTexture(const TargetDescription &desc) -> unsigned int {
  return texturePool.Request(desc);
}
auto GLResourceManager::Clear() -> void {
  // TODO: release OpenGL resources here
}
auto GLResourceManager::CreateBuffer(std::string name, const BufferDescription &desc) -> EntityID {
  if (auto id = resourceManager.GetID(name); id)
    return id;
  const auto resourceId = resourceManager.Create(name);
  auto buffer = resourceManager.AddComponent<GLBuffer>(resourceId);
  buffer->id = BorrowBuffer(desc);
  return resourceId;
}
auto GLResourceManager::LoadPrimitive(const std::string &name) -> EntityID {
  if (resourceManager.IsEntity(name))
    return resourceManager.GetID(name);
  std::vector<Vertex> vertices;
  if (name == "Cube")
    vertices = std::move(Primitive::Cube());
  else if (name == "CubeInverted") {
    vertices = std::move(Primitive::Cube());
    Primitive::FlipWindingOrder(vertices);
  } else if (name == "Frame")
    vertices = std::move(Primitive::Frame());
  else if (name == "Plane")
    vertices = std::move(Primitive::Plane());
  else if (name == "Cylinder")
    vertices = std::move(Primitive::Cylinder());
  else if (name == "Sphere")
    vertices = std::move(Primitive::Sphere());
  else
    return EntityID::Invalid;
  const auto resourceId = resourceManager.Create(name);
  auto mesh = resourceManager.AddComponent<GLMesh>(resourceId);
  CreateVertexBuffer(*mesh, vertices);
  CalculateBounds(*mesh, vertices);
  return resourceId;
}
auto GLResourceManager::CreateTarget(std::string name, const TargetDescription &desc) -> EntityID {
  if (auto id = resourceManager.GetID(name); id)
    return id;
  const auto resourceId = resourceManager.Create(name);
  auto target = resourceManager.AddComponent<GLRenderTarget>(resourceId);
  target->framebuffer = BorrowFramebuffer();
  target->renderbuffer = BorrowRenderbuffer(desc);
  target->texture = BorrowTexture(desc);
  glBindFramebuffer(GL_FRAMEBUFFER, target->framebuffer);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, target->renderbuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target->texture, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return resourceId;
}
auto GLResourceManager::GetResourceID(const std::string &name) const -> EntityID {
  return resourceManager.GetID(name);
}
auto GLResourceManager::CalculateBounds(GLMesh &mesh, const std::vector<Vertex> &vertices) -> void {
  mesh.bounds.min = glm::vec3(std::numeric_limits<float>::max());
  mesh.bounds.max = glm::vec3(std::numeric_limits<float>::lowest());
  for (const auto &vertex : vertices) {
    mesh.bounds.min = glm::min(mesh.bounds.min, vertex.position);
    mesh.bounds.max = glm::max(mesh.bounds.max, vertex.position);
  }
}
auto GLResourceManager::CreateIndexBuffer(GLMesh &mesh, const std::vector<unsigned int> &indices) -> void {
  mesh.indexCount = indices.size();
  GLuint indexBuffer;
  glCreateBuffers(1, &indexBuffer);
  mesh.ebo = indexBuffer;
  glNamedBufferData(mesh.ebo, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
  glVertexArrayElementBuffer(mesh.vao, mesh.ebo);
}
auto GLResourceManager::CreateVertexBuffer(GLMesh &mesh, const std::vector<Vertex> &vertices, bool skinned) -> void {
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
auto GLResourceManager::PrepareScene(Scene &scene) -> void {
  scene.ForEachEntity<MeshHandle, MaterialHandle>([&](const EntityID, MeshHandle *meshHandle, MaterialHandle *materialHandle) {
    if (meshHandle->resourceId && materialHandle->resourceId)
      return;
    auto sceneAsset = assetManager.Get<SceneAsset>(meshHandle->sceneAssetId);
    if (!sceneAsset)
      return;
    if (!meshHandle->resourceId)
      meshHandle->resourceId = LoadSceneMesh(*sceneAsset, meshHandle->meshIndex);
    if (!materialHandle->resourceId)
      materialHandle->resourceId = LoadSceneMaterial(*sceneAsset, materialHandle->materialIndex);
  });
  scene.ForEachEntity<MeshHandle, MaterialHandle>([&](const EntityID id, MeshHandle *meshHandle, MaterialHandle *materialHandle) {
    // FIXME: the following doesn't work for some reason
    auto [mesh, material] = scene.AddEntityComponent<GLMesh, GLMaterial>(id);
    //auto mesh = scene.AddEntityComponent<GLMesh>(id);
    //auto material = scene.AddEntityComponent<GLMaterial>(id);
    *mesh = *resourceManager.GetComponent<GLMesh>(meshHandle->resourceId);
    *material = *resourceManager.GetComponent<GLMaterial>(materialHandle->resourceId);
    // FIXME: no need to re-assign if the component already exists
  });
}
auto GLResourceManager::LoadSceneMaterial(SceneAsset &sceneAsset, const size_t materialIndex) -> EntityID {
  if (materialIndex >= sceneAsset.materials.size())
    return EntityID::Invalid;
  auto &sceneMaterial = sceneAsset.materials[materialIndex];
  if (sceneMaterial.resourceId)
    return sceneMaterial.resourceId;
  const auto &resourceId = resourceManager.Create(sceneMaterial.name);
  sceneMaterial.resourceId = resourceId;
  auto material = resourceManager.AddComponent<GLMaterial>(resourceId);
  material->fallback = sceneMaterial.fallback;
  for (auto i = 0; i < sceneMaterial.textures.size(); ++i) {
    const auto &textureIndex = sceneMaterial.textures[i];
    const auto texture = LoadSceneTexture(sceneAsset, textureIndex);
    if (!texture)
      continue;
    switch (texture->content) {
    case TextureContent::Albedo:
      material->textures.albedo = texture->id;
      material->fallback.textureMask.set(static_cast<int>(TextureContent::Albedo));
      break;
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
      break;
    }
  }
  return resourceId;
}
auto GLResourceManager::LoadSceneMesh(SceneAsset &sceneAsset, const size_t meshIndex) -> EntityID {
  if (meshIndex >= sceneAsset.meshes.size())
    return EntityID::Invalid;
  auto &sceneMesh = sceneAsset.meshes[meshIndex];
  if (sceneMesh.resourceId)
    return sceneMesh.resourceId;
  const auto resourceId = resourceManager.Create(sceneMesh.name);
  sceneMesh.resourceId = resourceId;
  auto mesh = resourceManager.AddComponent<GLMesh>(resourceId);
  CreateVertexBuffer(*mesh, sceneMesh.vertices);
  if (sceneMesh.indices.size() > 0)
    CreateIndexBuffer(*mesh, sceneMesh.indices);
  CalculateBounds(*mesh, sceneMesh.vertices);
  const auto &materialIndex = sceneMesh.material;
  LoadSceneMaterial(sceneAsset, materialIndex);
  return resourceId;
}
auto GLResourceManager::LoadSceneTexture(SceneAsset &sceneAsset, const size_t textureIndex) -> GLTexture * {
  if (textureIndex >= sceneAsset.textures.size())
    return nullptr;
  auto &sceneTexture = sceneAsset.textures[textureIndex];
  if (sceneTexture.resourceId)
    return resourceManager.GetComponent<GLTexture>(sceneTexture.resourceId);
  const auto resourceId = resourceManager.Create(sceneTexture.name);
  sceneTexture.resourceId = resourceId;
  auto texture = resourceManager.AddComponent<GLTexture>(resourceId);
  texture->desc.width = sceneTexture.width;
  texture->desc.height = sceneTexture.height;
  texture->content = sceneTexture.content;
  const auto isHDR = sceneTexture.color == ColorSpace::HDR;
  const auto isSRGB = sceneTexture.color == ColorSpace::sRGB;
  GLenum internalFormat, format;
  switch (sceneTexture.channels) {
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
    break;
  }
  texture->desc.format = GLFormatToTarget(internalFormat);
  glCreateTextures(GL_TEXTURE_2D, 1, &texture->id);
  auto mipmaps = 1;
  if (isSRGB)
    mipmaps = std::log2(std::max(sceneTexture.width, sceneTexture.height)) + 1;
  const auto type = isHDR ? GL_FLOAT : GL_UNSIGNED_BYTE;
  glTextureStorage2D(texture->id, mipmaps, internalFormat, sceneTexture.width, sceneTexture.height);
  glTextureSubImage2D(texture->id, 0, 0, 0, sceneTexture.width, sceneTexture.height, format, type, sceneTexture.data.data());
  switch (sceneTexture.type) {
  case TextureType::UV2D:
  case TextureType::Equirectangular:
    glTextureParameteri(texture->id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture->id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture->id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    break;
  default:
    glTextureParameteri(texture->id, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(texture->id, GL_TEXTURE_WRAP_T, GL_REPEAT);
    if (sceneTexture.channels == 1) {
      glTextureParameteri(texture->id, GL_TEXTURE_SWIZZLE_G, GL_RED);
      glTextureParameteri(texture->id, GL_TEXTURE_SWIZZLE_B, GL_RED);
      glTextureParameteri(texture->id, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    }
    glTextureParameteri(texture->id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateTextureMipmap(texture->id);
    break;
  }
  return texture;
}
} // namespace kuki
