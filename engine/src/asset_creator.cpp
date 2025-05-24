#include <glad/glad.h>
#include <application.hpp>
#include <asset_loader.hpp>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/types.h>
#include <component/component.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/skybox.hpp>
#include <component/texture.hpp>
#include <cstddef>
#include <entity_manager.hpp>
#include <glm/detail/type_vec2.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <limits>
#include <primitive.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <variant>
#include <vector>
#include <cmath>
#include <string>
namespace kuki {
Texture AssetLoader::CreateTexture(const TextureData& textureData) {
  Texture texture{};
  if (!textureData.data) {
    spdlog::error("Failed to read texture data.");
    return texture;
  }
  GLenum internalFormat, format;
  auto isHDR = textureData.type == TextureType::HDR || textureData.type == TextureType::EXR;
  auto invalidChannels = false;
  switch (textureData.channels) {
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
    else if (textureData.type == TextureType::Albedo)
      internalFormat = GL_SRGB8;
    else
      internalFormat = GL_RGB8;
    format = GL_RGB;
    break;
  case 4:
    if (isHDR)
      internalFormat = GL_RGBA16F;
    else if (textureData.type == TextureType::Albedo)
      internalFormat = GL_SRGB8_ALPHA8;
    else
      internalFormat = GL_RGBA8;
    format = GL_RGBA;
    break;
  default:
    invalidChannels = true;
    spdlog::error("Unsupported number of texture channels: {}.", textureData.channels);
    break;
  }
  unsigned int textureId;
  if (!invalidChannels) {
    glCreateTextures(GL_TEXTURE_2D, 1, &textureId);
    int levels = 1;
    if (textureData.type == TextureType::Albedo)
      levels = std::log2(std::max(textureData.width, textureData.height)) + 1;
    glTextureStorage2D(textureId, levels, internalFormat, textureData.width, textureData.height);
    glTextureSubImage2D(textureId, 0, 0, 0, textureData.width, textureData.height, format, isHDR ? GL_FLOAT : GL_UNSIGNED_BYTE, textureData.data);
  }
  stbi_image_free(textureData.data);
  if (invalidChannels)
    return texture;
  switch (textureData.type) {
  case TextureType::Albedo:
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateTextureMipmap(textureId);
    break;
  case TextureType::Normal:
  case TextureType::BRDF:
  case TextureType::HDR:
  case TextureType::EXR:
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    break;
  case TextureType::Roughness:
  case TextureType::Metalness:
  case TextureType::Occlusion:
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    if (textureData.channels == 1) {
      glTextureParameteri(textureId, GL_TEXTURE_SWIZZLE_G, GL_RED);
      glTextureParameteri(textureId, GL_TEXTURE_SWIZZLE_B, GL_RED);
      glTextureParameteri(textureId, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    }
    break;
  default:
    spdlog::error("Unknown texture type: {}.", static_cast<int>(textureData.type));
    break;
  }
  texture.type = textureData.type;
  texture.width = textureData.width;
  texture.height = textureData.height;
  texture.id = textureId;
  return texture;
}
int AssetLoader::CreateTextureAsset(const TextureData& textureData) {
  auto isHDR = textureData.type == TextureType::HDR || textureData.type == TextureType::EXR;
  if (isHDR)
    return CreateSkyboxAsset(textureData);
  auto texture = CreateTexture(textureData);
  auto name = textureData.name;
  auto assetId = assetManager.Create(name);
  auto textureComp = assetManager.AddComponent<Texture>(assetId);
  *textureComp = texture;
  spdlog::info("Texture is created: {}.", name);
  return assetId;
}
int AssetLoader::CreateSkyboxAsset(const TextureData& textureData) {
  auto texture = CreateTexture(textureData);
  std::string name = textureData.name + "Skybox";
  auto assetId = app->CreateAsset(name);
  auto skybox = app->AddAssetComponent<Skybox>(assetId);
  auto cubeMap = app->CreateCubeMapFromEquirect(texture);
  skybox->data.skybox = cubeMap.id;
  skybox->data.irradiance = app->CreateIrradianceMapFromCubeMap(cubeMap).id;
  skybox->data.prefilter = app->CreatePrefilterMapFromCubeMap(cubeMap).id;
  skybox->data.brdf = app->CreateBRDF_LUT().id;
  spdlog::info("Skybox is created: {}.", name);
  return assetId;
}
Material AssetLoader::CreateMaterial(const MaterialData& materialData) {
  Material material{};
  material.material = LitMaterial{};
  auto litMaterial = std::get_if<LitMaterial>(&material.material);
  litMaterial->fallback.textureMask = materialData.textureMask;
  litMaterial->fallback.albedo = materialData.albedo;
  litMaterial->fallback.specular = materialData.specular;
  litMaterial->fallback.metalness = materialData.metalness;
  litMaterial->fallback.roughness = materialData.roughness;
  litMaterial->fallback.occlusion = materialData.occlusion;
  for (auto i = 0; i < materialData.textureData.size(); ++i) {
    if (!materialData.textureData[i].data)
      continue;
    auto assetId = CreateTextureAsset(materialData.textureData[i]);
    auto texture = assetManager.GetComponent<Texture>(assetId);
    if (!texture || texture->id == 0)
      continue;
    if (litMaterial) {
      switch (texture->type) {
      case TextureType::Albedo:
        litMaterial->data.albedo = texture->id;
        break;
      case TextureType::Normal:
        litMaterial->data.normal = texture->id;
        break;
      case TextureType::Metalness:
        litMaterial->data.metalness = texture->id;
        break;
      case TextureType::Occlusion:
        litMaterial->data.occlusion = texture->id;
        break;
      case TextureType::Roughness:
        litMaterial->data.roughness = texture->id;
        break;
      case TextureType::Specular:
        litMaterial->data.specular = texture->id;
        break;
      case TextureType::Emissive:
        litMaterial->data.emissive = texture->id;
        break;
      default:
        break;
      }
    }
  }
  return material;
}
int AssetLoader::CreateMaterialAsset(const MaterialData& materialData) {
  auto name = materialData.name;
  auto assetId = assetManager.Create(name);
  auto material = assetManager.AddComponent<Material>(assetId);
  *material = CreateMaterial(materialData);
  return assetId;
}
Mesh AssetLoader::CreateMesh(const aiMesh* aiMesh) {
  std::vector<Vertex> vertices;
  for (auto i = 0; i < aiMesh->mNumVertices; ++i) {
    Vertex vertex{};
    glm::vec3 vector{};
    vector.x = aiMesh->mVertices[i].x;
    vector.y = aiMesh->mVertices[i].y;
    vector.z = aiMesh->mVertices[i].z;
    vertex.position = vector;
    vector.x = aiMesh->mNormals[i].x;
    vector.y = aiMesh->mNormals[i].y;
    vector.z = aiMesh->mNormals[i].z;
    vertex.normal = vector;
    vector.x = aiMesh->mTangents[i].x;
    vector.y = aiMesh->mTangents[i].y;
    vector.z = aiMesh->mTangents[i].z;
    vertex.tangent = vector;
    if (aiMesh->mTextureCoords[0]) {
      glm::vec2 texCoord{};
      texCoord.x = aiMesh->mTextureCoords[0][i].x;
      texCoord.y = 1.0f - aiMesh->mTextureCoords[0][i].y;
      vertex.texture = texCoord;
    } else
      vertex.texture = glm::vec2(.0f);
    vertices.push_back(vertex);
  }
  std::vector<unsigned int> indices;
  for (auto i = 0; i < aiMesh->mNumFaces; ++i) {
    auto& face = aiMesh->mFaces[i];
    for (auto j = 0; j < face.mNumIndices; ++j)
      indices.push_back(face.mIndices[j]);
  }
  return CreateMesh(vertices, indices);
}
Mesh AssetLoader::CreateMesh(const std::vector<Vertex>& vertices) {
  auto mesh = CreateVertexBuffer(vertices);
  CalculateBounds(mesh, vertices);
  return mesh;
}
Mesh AssetLoader::CreateMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
  auto mesh = CreateMesh(vertices);
  CreateIndexBuffer(mesh, indices);
  return mesh;
}
Mesh AssetLoader::CreateVertexBuffer(const std::vector<Vertex>& vertices) {
  Mesh mesh;
  mesh.vertexCount = vertices.size();
  unsigned int vertexArray, vertexBuffer;
  glCreateVertexArrays(1, &vertexArray);
  glCreateBuffers(1, &vertexBuffer);
  mesh.vertexArray = vertexArray;
  mesh.vertexBuffer = vertexBuffer;
  auto bindingIndex = 0;
  glNamedBufferData(mesh.vertexBuffer, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
  glVertexArrayVertexBuffer(mesh.vertexArray, bindingIndex, mesh.vertexBuffer, 0, sizeof(Vertex));
  auto attribIndex = 0;
  glVertexArrayAttribFormat(mesh.vertexArray, attribIndex, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
  glVertexArrayAttribBinding(mesh.vertexArray, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vertexArray, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh.vertexArray, attribIndex, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
  glVertexArrayAttribBinding(mesh.vertexArray, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vertexArray, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh.vertexArray, attribIndex, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, texture));
  glVertexArrayAttribBinding(mesh.vertexArray, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vertexArray, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh.vertexArray, attribIndex, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, tangent));
  glVertexArrayAttribBinding(mesh.vertexArray, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vertexArray, attribIndex);
  return mesh;
}
void AssetLoader::CreateIndexBuffer(Mesh& mesh, const std::vector<unsigned int>& indices) {
  mesh.indexCount = indices.size();
  unsigned int indexBuffer;
  glCreateBuffers(1, &indexBuffer);
  mesh.indexBuffer = indexBuffer;
  glNamedBufferData(mesh.indexBuffer, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
  glVertexArrayElementBuffer(mesh.vertexArray, mesh.indexBuffer);
}
void AssetLoader::CalculateBounds(Mesh& mesh, const std::vector<Vertex>& vertices) {
  mesh.bounds.min = glm::vec3(std::numeric_limits<float>::max());
  mesh.bounds.max = glm::vec3(std::numeric_limits<float>::lowest());
  for (const auto& vertex : vertices) {
    mesh.bounds.min = glm::min(mesh.bounds.min, vertex.position);
    mesh.bounds.max = glm::max(mesh.bounds.max, vertex.position);
  }
}
} // namespace kuki
