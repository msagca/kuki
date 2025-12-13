#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>
#include <application.hpp>
#include <asset_loader.hpp>
#include <assimp/mesh.h>
#include <assimp/types.h>
#include <assimp/vector3.h>
#include <bone_data.hpp>
#include <cmath>
#include <component.hpp>
#include <cstddef>
#include <entity_manager.hpp>
#include <glad/glad.h>
#include <glm/detail/type_vec2.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/ext/vector_int4.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <id.hpp>
#include <limits>
#include <material.hpp>
#include <mesh.hpp>
#include <primitive.hpp>
#include <skybox.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <string>
#include <texture.hpp>
#include <transform.hpp>
#include <unordered_map>
#include <variant>
#include <vector>
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
ID AssetLoader::CreateTextureAsset(const TextureData& textureData) {
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
ID AssetLoader::CreateSkyboxAsset(const TextureData& textureData) {
  auto texture = CreateTexture(textureData);
  std::string name = textureData.name + "Skybox";
  auto assetId = app->CreateAsset(name);
  auto skybox = app->AddAssetComponent<Skybox>(assetId);
  auto cubeMap = app->CreateCubeMapFromEquirect(texture);
  skybox->skybox = cubeMap.id;
  skybox->irradiance = app->CreateIrradianceMapFromCubeMap(cubeMap).id;
  skybox->prefilter = app->CreatePrefilterMapFromCubeMap(cubeMap).id;
  skybox->brdf = app->CreateBRDF_LUT().id;
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
ID AssetLoader::CreateMaterialAsset(const MaterialData& materialData) {
  auto name = materialData.name;
  auto assetId = assetManager.Create(name);
  auto material = assetManager.AddComponent<Material>(assetId);
  auto newMaterial = CreateMaterial(materialData);
  *material = newMaterial;
  return assetId;
}
Mesh AssetLoader::CreateMesh(const aiMesh* aiMesh) {
  std::vector<Vertex> vertices;
  for (auto i = 0; i < aiMesh->mNumVertices; ++i) {
    Vertex vertex{};
    vertex.position = glm::vec3(aiMesh->mVertices[i].x, aiMesh->mVertices[i].y, aiMesh->mVertices[i].z);
    vertex.normal = glm::vec3(aiMesh->mNormals[i].x, aiMesh->mNormals[i].y, aiMesh->mNormals[i].z);
    // FIXME: some models do not have tangents, and the following access results in an exception
    vertex.tangent = glm::vec3(aiMesh->mTangents[i].x, aiMesh->mTangents[i].y, aiMesh->mTangents[i].z);
    vertex.boneIds = glm::ivec4(-1); // NOTE: -1 indicates that this hasn't been assigned yet, will be used later
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
  auto vertexCount = vertices.size();
  std::unordered_map<std::string, unsigned int> boneNameToId;
  for (auto i = 0; i < aiMesh->mNumBones; ++i) {
    auto bone = aiMesh->mBones[i];
    auto boneName(bone->mName.C_Str());
    auto boneId = 0u;
    auto boneNameId = boneNameToId.find(boneName);
    if (boneNameId != boneNameToId.end())
      boneId = boneNameId->second;
    else {
      boneId = static_cast<unsigned int>(boneNameToId.size());
      boneNameToId[boneName] = boneId;
    }
    for (auto j = 0; j < bone->mNumWeights; ++j) {
      auto vertexId = bone->mWeights[j].mVertexId;
      if (vertexId < vertexCount)
        continue;
      auto weight = bone->mWeights[j].mWeight;
      for (auto k = 0; k < 4; ++k)
        // assign the weight to the next unassigned id
        // TODO: use the largest 4 weights by normalizing their sum to 1
        if (vertices[vertexId].boneIds[k] < 0) {
          vertices[vertexId].boneIds[k] = boneId;
          vertices[vertexId].boneWeights[k] = weight;
          break;
        }
    }
  }
  return CreateMesh(vertices, indices);
}
BoneData AssetLoader::CreateBoneData(const aiMesh* aiMesh) {
  std::unordered_map<std::string, unsigned int> boneNameToId;
  std::vector<glm::mat4> boneTransforms;
  for (auto i = 0; i < aiMesh->mNumBones; ++i) {
    auto bone = aiMesh->mBones[i];
    auto boneName(bone->mName.C_Str());
    auto boneId = 0u;
    auto boneNameId = boneNameToId.find(boneName);
    if (boneNameId != boneNameToId.end())
      boneId = boneNameId->second;
    else {
      boneId = static_cast<unsigned int>(boneNameToId.size());
      boneNameToId[boneName] = boneId;
      boneTransforms.push_back(AssimpToGlmMat4(bone->mOffsetMatrix));
    }
  }
  BoneData boneData;
  if (boneTransforms.size() > 0) {
    GLuint ssbo;
    glCreateBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(boneTransforms), boneTransforms.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    boneData.boneSSBO = ssbo;
    boneData.boneCount = boneNameToId.size();
  }
  return boneData;
}
ID AssetLoader::CreateNode(const NodeData& nodeData) {
  auto name = nodeData.name;
  const auto assetId = assetManager.Create(name);
  auto transform = assetManager.AddComponent<Transform>(assetId);
  glm::vec3 skew;
  glm::vec4 perspective;
  glm::decompose(nodeData.transform, transform->scale, transform->rotation, transform->position, skew, perspective);
  /*auto normalized = nodeData.transform * glm::scale(glm::vec3(1.0f) / transform->scale);
  transform->scale = glm::vec3(1.0f);
  transform->local = normalized;*/
  transform->local = nodeData.transform;
  assetManager.AddChild(nodeData.parent, assetId);
  if (nodeData.hasMesh) {
    auto mesh = assetManager.AddComponent<Mesh>(assetId);
    *mesh = nodeData.mesh;
  }
  if (nodeData.hasMaterial) {
    auto material = assetManager.AddComponent<Material>(assetId);
    *material = nodeData.material;
  }
  if (nodeData.hasBoneData) {
    auto boneData = assetManager.AddComponent<BoneData>(assetId);
    *boneData = nodeData.boneData;
  }
  return assetId;
}
Mesh AssetLoader::CreateMesh(const std::vector<Vertex>& vertices, bool skinned) {
  Mesh mesh;
  CreateVertexBuffer(mesh, vertices, skinned);
  CalculateBounds(mesh, vertices);
  return mesh;
}
Mesh AssetLoader::CreateMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, bool skinned) {
  auto mesh = CreateMesh(vertices, skinned);
  CreateIndexBuffer(mesh, indices);
  return mesh;
}
Mesh AssetLoader::CreateVertexBuffer(Mesh& mesh, const std::vector<Vertex>& vertices, bool skinned) {
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
  return mesh;
}
void AssetLoader::CreateIndexBuffer(Mesh& mesh, const std::vector<unsigned int>& indices) {
  mesh.indexCount = indices.size();
  GLuint indexBuffer;
  glCreateBuffers(1, &indexBuffer);
  mesh.ebo = indexBuffer;
  glNamedBufferData(mesh.ebo, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
  glVertexArrayElementBuffer(mesh.vao, mesh.ebo);
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
