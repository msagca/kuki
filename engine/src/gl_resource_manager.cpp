#include <buffer_description.hpp>
#include <color_space.hpp>
#include <gl_buffer.hpp>
#include <gl_compute_shader.hpp>
#include <gl_framebuffer.hpp>
#include <gl_render_target.hpp>
#include <gl_resource_manager.hpp>
#include <gl_shader.hpp>
#include <gl_texture.hpp>
#include <id.hpp>
#include <spdlog/spdlog.h>
//
#include <glad/glad.h>
namespace kuki {
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
  // TODO: return OpenGL resources here
}
auto GLResourceManager::CreateBuffer(std::string name, const BufferDescription &desc) -> EntityID {
  if (auto id = entityManager.IsEntity(name); id)
    return id;
  const auto entityId = entityManager.Create(name);
  auto bufferObject = entityManager.AddComponent<GLBuffer>(entityId);
  bufferObject->id = BorrowBuffer(desc);
  return entityId;
}
auto GLResourceManager::CreateCompute(const ComputeType type, const ShaderAsset &comp) -> EntityID {
  const auto assetId = comp.GetID();
  if (!assetId)
    return EntityID::Invalid;
  if (auto it = assetToEntityId.find(assetId); it != assetToEntityId.end())
    return it->second;
  const auto entityId = entityManager.Create();
  assetToEntityId[assetId] = entityId;
  auto compute = entityManager.AddComponent<GLComputeShader>(entityId);
  compute->type = type;
  auto compId = GLShaderBase::Compile(comp.text.data(), GL_COMPUTE_SHADER);
  const auto programId = glCreateProgram();
  compute->id = programId;
  glAttachShader(programId, compId);
  glLinkProgram(programId);
  glDeleteShader(compId);
  int success;
  glGetProgramiv(programId, GL_LINK_STATUS, &success);
  if (success)
    compute->CacheLocations();
  return entityId;
}
auto GLResourceManager::CreateMesh(std::string name, const MeshAsset &meshAsset) -> EntityID {
  if (auto id = entityManager.IsEntity(name); id)
    return id;
  const auto assetId = meshAsset.GetID();
  if (!assetId)
    return EntityID::Invalid;
  if (auto it = assetToEntityId.find(assetId); it != assetToEntityId.end())
    return it->second;
  const auto entityId = entityManager.Create(name);
  assetToEntityId[assetId] = entityId;
  auto mesh = entityManager.AddComponent<GLMesh>(entityId);
  CreateVertexBuffer(*mesh, meshAsset.vertices);
  if (meshAsset.indices.size() > 0)
    CreateIndexBuffer(*mesh, meshAsset.indices);
  CalculateBounds(*mesh, meshAsset.vertices);
  return entityId;
}
auto GLResourceManager::CreatePrimitive(const PrimitiveType type) -> EntityID {
  if (primitiveToAssetId.contains(type))
    return EntityID::Invalid;
  MeshAsset meshAsset{AssetID::Generate()};
  std::string name;
  switch (type) {
  case PrimitiveType::Cube:
    meshAsset.vertices = Primitive::Cube();
    name = "Cube";
    break;
  case PrimitiveType::CubeInverted:
    meshAsset.vertices = Primitive::Cube();
    name = "CubeInverted";
    Primitive::FlipWindingOrder(meshAsset.vertices);
    break;
  case PrimitiveType::Cylinder:
    meshAsset.vertices = Primitive::Cylinder();
    name = "Cylinder";
    break;
  case PrimitiveType::Frame:
    meshAsset.vertices = Primitive::Frame();
    name = "Frame";
    break;
  case PrimitiveType::Plane:
    meshAsset.vertices = Primitive::Plane();
    name = "Plane";
    break;
  case PrimitiveType::Sphere:
    meshAsset.vertices = Primitive::Sphere();
    name = "Sphere";
    break;
  default:
    break;
  }
  if (meshAsset.vertices.empty())
    return EntityID::Invalid;
  const auto entityId = CreateMesh(name, meshAsset);
  primitiveToAssetId.emplace(type, meshAsset.GetID());
  return entityId;
}
auto GLResourceManager::CreateShader(const MaterialType type, const ShaderAsset &vert, const ShaderAsset &frag) -> EntityID {
  const auto assetId = vert.GetID();
  if (!assetId)
    return EntityID::Invalid;
  if (auto it = assetToEntityId.find(assetId); it != assetToEntityId.end())
    return it->second;
  const auto entityId = entityManager.Create();
  assetToEntityId[assetId] = entityId;
  auto shader = entityManager.AddComponent<GLShader>(entityId);
  shader->type = type;
  auto vertId = GLShaderBase::Compile(vert.text.data(), GL_VERTEX_SHADER);
  auto fragId = GLShaderBase::Compile(frag.text.data(), GL_FRAGMENT_SHADER);
  const auto programId = glCreateProgram();
  shader->id = programId;
  glAttachShader(programId, vertId);
  glAttachShader(programId, fragId);
  glLinkProgram(programId);
  glDeleteShader(vertId);
  glDeleteShader(fragId);
  int success;
  glGetProgramiv(programId, GL_LINK_STATUS, &success);
  if (success)
    shader->CacheLocations();
  return entityId;
}
auto GLResourceManager::CreateTarget(std::string name, const TargetDescription &desc) -> EntityID {
  if (auto id = entityManager.IsEntity(name); id)
    return id;
  const auto entityId = entityManager.Create(name);
  auto renderTarget = entityManager.AddComponent<GLRenderTarget>(entityId);
  renderTarget->framebuffer = BorrowFramebuffer();
  renderTarget->renderbuffer = BorrowRenderbuffer(desc);
  renderTarget->texture = BorrowTexture(desc);
  return entityId;
}
auto GLResourceManager::CreateTexture(std::string name, const TextureAsset &textureAsset) -> EntityID {
  if (auto id = entityManager.IsEntity(name); id)
    return id;
  const auto assetId = textureAsset.GetID();
  if (!assetId)
    return EntityID::Invalid;
  if (auto it = assetToEntityId.find(assetId); it != assetToEntityId.end())
    return it->second;
  const auto isHDR = textureAsset.color == ColorSpace::HDR;
  const auto isSRGB = textureAsset.color == ColorSpace::sRGB;
  const auto entityId = entityManager.Create(name);
  assetToEntityId[assetId] = entityId;
  auto texture = entityManager.AddComponent<GLTexture>(entityId);
  GLenum internalFormat, format;
  auto invalidChannels = false;
  switch (textureAsset.channels) {
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
    invalidChannels = true;
    break;
  }
  GLuint textureId;
  glCreateTextures(GL_TEXTURE_2D, 1, &textureId);
  auto mipmaps = 1;
  if (isSRGB)
    mipmaps = std::log2(std::max(textureAsset.width, textureAsset.height)) + 1;
  const auto type = isHDR ? GL_FLOAT : GL_UNSIGNED_BYTE;
  glTextureStorage2D(textureId, mipmaps, internalFormat, textureAsset.width, textureAsset.height);
  glTextureSubImage2D(textureId, 0, 0, 0, textureAsset.width, textureAsset.height, format, type, textureAsset.data.data());
  switch (textureAsset.type) {
  case TextureType::UV2D:
  case TextureType::Equirectangular:
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    break;
  default:
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_T, GL_REPEAT);
    if (textureAsset.channels == 1) {
      glTextureParameteri(textureId, GL_TEXTURE_SWIZZLE_G, GL_RED);
      glTextureParameteri(textureId, GL_TEXTURE_SWIZZLE_B, GL_RED);
      glTextureParameteri(textureId, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    }
    glTextureParameteri(textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateTextureMipmap(textureId);
    break;
  }
  texture->id = textureId;
  return entityId;
}
auto GLResourceManager::GetBuffer(const EntityID id) -> GLBuffer * {
  return entityManager.GetComponent<GLBuffer>(id);
}
auto GLResourceManager::GetBuffer(const std::string &name) -> GLBuffer * {
  const auto id = entityManager.GetID(name);
  return entityManager.GetComponent<GLBuffer>(id);
}
auto GLResourceManager::GetCompute(const ComputeType type) -> GLComputeShader * {
  if (auto it = computeToEntityId.find(type); it != computeToEntityId.end())
    return entityManager.GetComponent<GLComputeShader>(it->second);
  return nullptr;
}
auto GLResourceManager::GetID(const std::string &name) -> EntityID {
  return entityManager.GetID(name);
}
auto GLResourceManager::GetMesh(const EntityID id) -> GLMesh * {
  return entityManager.GetComponent<GLMesh>(id);
}
auto GLResourceManager::GetMesh(const std::string &name) -> GLMesh * {
  const auto id = entityManager.GetID(name);
  return entityManager.GetComponent<GLMesh>(id);
}
auto GLResourceManager::GetPrimitive(const PrimitiveType type) -> GLMesh * {
  if (auto it = primitiveToAssetId.find(type); it != primitiveToAssetId.end())
    if (auto it2 = assetToEntityId.find(it->second); it2 != assetToEntityId.end())
      return entityManager.GetComponent<GLMesh>(it2->second);
  return nullptr;
}
auto GLResourceManager::GetShader(const MaterialType type) -> GLShader * {
  if (auto it = shaderToEntityId.find(type); it != shaderToEntityId.end())
    return entityManager.GetComponent<GLShader>(it->second);
  return nullptr;
}
auto GLResourceManager::GetTarget(const EntityID id) -> GLRenderTarget * {
  return entityManager.GetComponent<GLRenderTarget>(id);
}
auto GLResourceManager::GetTarget(const std::string &name) -> GLRenderTarget * {
  const auto id = entityManager.GetID(name);
  return entityManager.GetComponent<GLRenderTarget>(id);
}
auto GLResourceManager::GetTexture(const EntityID id) -> GLTexture * {
  return entityManager.GetComponent<GLTexture>(id);
}
auto GLResourceManager::GetTexture(const std::string &name) -> GLTexture * {
  const auto id = entityManager.GetID(name);
  return entityManager.GetComponent<GLTexture>(id);
}
auto GLResourceManager::TargetFormatToGL(const TargetFormat &format) -> unsigned int {
  switch (format) {
  case TargetFormat::R16:
    return GL_R16F;
  case TargetFormat::RGB16:
    return GL_RGB16F;
  case TargetFormat::RGB32:
    return GL_RGB32F;
  case TargetFormat::RGBA16:
    return GL_RGBA16F;
  case TargetFormat::RGBA32:
    return GL_RGBA32F;
  default:
    return 0;
  }
}
auto GLResourceManager::TargetTypeToGL(const TargetType &target) -> unsigned int {
  switch (target) {
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
} // namespace kuki
