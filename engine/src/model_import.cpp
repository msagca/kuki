#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>
#include <animator.hpp>
#include <assimp/GltfMaterial.h>
#include <assimp/color4.h>
#include <assimp/material.h>
#include <assimp/matrix4x4.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <bounding_box.hpp>
#include <cmath>
#include <color.hpp>
#include <filesystem>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/ext/vector_int4.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <limits>
#include <model_asset.hpp>
#include <model_import.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <string>
#include <string_view>
#include <texture.hpp>
#include <texture_content.hpp>
#include <unordered_map>
#include <utility>
namespace kuki {
/// @brief Cache entry meaning "this slot was looked at and there was no usable texture".
///
/// Distinguished from an absent entry so a failed load is not retried once per material that
/// references it.
constexpr auto MISSING_TEXTURE = std::numeric_limits<unsigned int>::max();
// Declared here rather than in the header: their signatures name `aiMatrix4x4` and `aiTextureType`,
// which cannot be forward declared, and they are only ever called from this file. The declarations
// carry the default arguments, since the definitions below do not repeat them.
auto AssimpToGlmMat4(const aiMatrix4x4 &) -> glm::mat4;
auto AssimpTexToContent(const aiTextureType) -> TextureContent;
auto LoadMaterial(std::unordered_map<std::string, unsigned int> &, const aiMaterial &, ModelAsset &, const aiScene &, const std::filesystem::path & = {}, const std::string & = {}) -> unsigned int;
auto LoadMesh(const aiMesh &, ModelAsset &, BoundingBox &, unsigned int, const std::string & = {}) -> unsigned int;
auto LoadTexture(std::unordered_map<std::string, unsigned int> &, const aiMaterial &, const aiTextureType, ModelMaterial &, ModelAsset &, const aiScene &, const std::filesystem::path &, const std::string & = {}) -> void;
auto AssimpToGlmMat4(const aiMatrix4x4 &m) -> glm::mat4 {
  return {m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3, m.c3, m.d3, m.a4, m.b4, m.c4, m.d4};
}
auto AssimpTexToContent(const aiTextureType t) -> TextureContent {
  switch (t) {
  case aiTextureType_NORMALS:
    return TextureContent::Normal;
  case aiTextureType_METALNESS:
    return TextureContent::Metalness;
  case aiTextureType_AMBIENT_OCCLUSION:
    return TextureContent::Occlusion;
  case aiTextureType_DIFFUSE_ROUGHNESS:
    return TextureContent::Roughness;
  case aiTextureType_SPECULAR:
    return TextureContent::Specular;
  case aiTextureType_EMISSIVE:
    return TextureContent::Emissive;
  default:
    return TextureContent::Albedo;
  }
}
auto LoadMaterial(std::unordered_map<std::string, unsigned int> &visited, const aiMaterial &aiMaterial, ModelAsset &model, const aiScene &aiScene, const std::filesystem::path &path, const std::string &fallbackName) -> unsigned int {
  const auto aiName = aiMaterial.GetName();
  const auto name = aiName.Empty() ? path.lexically_normal().string() : aiName.C_Str();
  if (auto it = visited.find(name); it != visited.end())
    return it->second;
  const unsigned int index = model.materials.size();
  visited.insert({name, index});
  model.materials.emplace_back();
  auto &material = model.materials.back();
  material.name = name;
  int shadingModel;
  if (aiMaterial.Get(AI_MATKEY_SHADING_MODEL, shadingModel) == AI_SUCCESS) {
    if (shadingModel == aiShadingMode_NoShading)
      material.type = MaterialType::Unlit;
    else
      material.type = MaterialType::Lit;
  }
  aiColor4D color;
  float value;
  if (aiMaterial.Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
    material.fallback.albedo = {color.r, color.g, color.b, color.a};
  if (aiMaterial.Get(AI_MATKEY_OPACITY, value) == AI_SUCCESS)
    material.fallback.albedo.w = value;
  if (aiString mode; aiMaterial.Get(AI_MATKEY_GLTF_ALPHAMODE, mode) == AI_SUCCESS) {
    const std::string_view name(mode.C_Str());
    if (name == "MASK")
      material.fallback.alphaMode = AlphaMode::Mask;
    else if (name == "BLEND")
      material.fallback.alphaMode = AlphaMode::Blend;
  } else if (material.fallback.albedo.w < 1.f)
    material.fallback.alphaMode = AlphaMode::Blend;
  if (aiMaterial.Get(AI_MATKEY_GLTF_ALPHACUTOFF, value) == AI_SUCCESS)
    material.fallback.alphaCutoff = value;
  if (aiMaterial.Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
    material.fallback.specular = {color.r, color.g, color.b, color.a};
  if (aiMaterial.Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
    material.fallback.emissive = {color.r, color.g, color.b, color.a};
  if (aiMaterial.Get(AI_MATKEY_METALLIC_FACTOR, value) == AI_SUCCESS || aiMaterial.Get(AI_MATKEY_REFLECTIVITY, value) == AI_SUCCESS)
    material.fallback.metalness = value;
  if (aiMaterial.Get(AI_MATKEY_ROUGHNESS_FACTOR, value) == AI_SUCCESS)
    material.fallback.roughness = value;
  else if (aiMaterial.Get(AI_MATKEY_SHININESS, value) == AI_SUCCESS)
    material.fallback.roughness = std::sqrt(2.f / (value + 2.f));
  if (aiMaterial.Get(AI_MATKEY_TRANSMISSION_FACTOR, value) == AI_SUCCESS)
    material.fallback.transmission = std::clamp(value, .0f, 1.f);
  if (aiMaterial.Get(AI_MATKEY_REFRACTI, value) == AI_SUCCESS && value >= 1.f)
    material.fallback.ior = value;
  if (aiMaterial.Get(AI_MATKEY_VOLUME_THICKNESS_FACTOR, value) == AI_SUCCESS)
    material.fallback.thickness = std::max(value, .0f);
  if (aiMaterial.Get(AI_MATKEY_VOLUME_ATTENUATION_DISTANCE, value) == AI_SUCCESS && std::isfinite(value))
    material.fallback.attenuation.w = std::max(value, .0f);
  if (aiColor3D attenuation; aiMaterial.Get(AI_MATKEY_VOLUME_ATTENUATION_COLOR, attenuation) == AI_SUCCESS)
    material.fallback.attenuation = {attenuation.r, attenuation.g, attenuation.b, material.fallback.attenuation.w};
  if (!path.empty()) {
    LoadTexture(visited, aiMaterial, aiTextureType_AMBIENT_OCCLUSION, material, model, aiScene, path, fallbackName);
    LoadTexture(visited, aiMaterial, aiTextureType_DIFFUSE, material, model, aiScene, path, fallbackName);
    LoadTexture(visited, aiMaterial, aiTextureType_DIFFUSE_ROUGHNESS, material, model, aiScene, path, fallbackName);
    LoadTexture(visited, aiMaterial, aiTextureType_EMISSIVE, material, model, aiScene, path, fallbackName);
    LoadTexture(visited, aiMaterial, aiTextureType_METALNESS, material, model, aiScene, path, fallbackName);
    LoadTexture(visited, aiMaterial, aiTextureType_NORMALS, material, model, aiScene, path, fallbackName);
    LoadTexture(visited, aiMaterial, aiTextureType_SPECULAR, material, model, aiScene, path, fallbackName);
  }
  return index;
}
auto LoadMesh(const aiMesh &aiMesh, ModelAsset &model, BoundingBox &bounds, unsigned int parent, const std::string &fallbackName) -> unsigned int {
  const unsigned int index = model.meshes.size();
  model.meshes.emplace_back();
  auto &modelMesh = model.meshes.back();
  modelMesh.name = aiMesh.mName.Empty() ? fallbackName : aiMesh.mName.C_Str();
  modelMesh.parent = parent;
  // both counts are known up front, so grow once rather than reallocating and moving log2(n) times
  modelMesh.mesh.vertices.reserve(aiMesh.mNumVertices);
  for (auto i = 0; i < aiMesh.mNumVertices; ++i) {
    modelMesh.mesh.vertices.emplace_back();
    auto &vertex = modelMesh.mesh.vertices.back();
    vertex.position = glm::vec3(aiMesh.mVertices[i].x, aiMesh.mVertices[i].y, aiMesh.mVertices[i].z);
    bounds.min = glm::min(bounds.min, vertex.position);
    bounds.max = glm::max(bounds.max, vertex.position);
    if (aiMesh.mNormals)
      vertex.normal = glm::vec3(aiMesh.mNormals[i].x, aiMesh.mNormals[i].y, aiMesh.mNormals[i].z);
    if (aiMesh.mTangents)
      vertex.tangent = glm::vec3(aiMesh.mTangents[i].x, aiMesh.mTangents[i].y, aiMesh.mTangents[i].z);
    vertex.boneIds = glm::ivec4(-1);
    if (aiMesh.mTextureCoords[0]) {
      glm::vec2 texCoord{};
      texCoord.x = aiMesh.mTextureCoords[0][i].x;
      texCoord.y = 1.f - aiMesh.mTextureCoords[0][i].y;
      vertex.texture = texCoord;
    } else
      vertex.texture = glm::vec2(0.f);
  }
  auto indexCount = 0u;
  for (auto i = 0; i < aiMesh.mNumFaces; ++i)
    indexCount += aiMesh.mFaces[i].mNumIndices;
  modelMesh.mesh.indices.reserve(indexCount);
  for (auto i = 0; i < aiMesh.mNumFaces; ++i) {
    const auto &face = aiMesh.mFaces[i];
    for (auto j = 0; j < face.mNumIndices; ++j)
      modelMesh.mesh.indices.push_back(face.mIndices[j]);
  }
  const auto vertexCount = modelMesh.mesh.vertices.size();
  std::unordered_map<std::string, int> boneNameToId;
  for (auto i = 0; i < aiMesh.mNumBones; ++i) {
    auto bone = aiMesh.mBones[i];
    const auto boneName(bone->mName.C_Str());
    auto boneId = 0u;
    if (auto it = boneNameToId.find(boneName); it != boneNameToId.end())
      boneId = it->second;
    else {
      boneId = static_cast<int>(boneNameToId.size());
      boneNameToId[boneName] = boneId;
      modelMesh.bones.push_back({.name = boneName, .offsetMatrix = AssimpToGlmMat4(bone->mOffsetMatrix)});
    }
    for (auto j = 0; j < bone->mNumWeights; ++j) {
      const auto vertexId = bone->mWeights[j].mVertexId;
      if (vertexId >= vertexCount)
        continue;
      const auto weight = bone->mWeights[j].mWeight;
      for (auto k = 0; k < 4; ++k)
        if (modelMesh.mesh.vertices[vertexId].boneIds[k] < 0) {
          modelMesh.mesh.vertices[vertexId].boneIds[k] = boneId;
          modelMesh.mesh.vertices[vertexId].boneWeights[k] = weight;
          break;
        }
    }
  }
  if (aiMesh.mNumBones > 0)
    for (auto &vertex : modelMesh.mesh.vertices)
      for (auto k = 0; k < 4; ++k)
        if (vertex.boneIds[k] < 0)
          vertex.boneIds[k] = 0;
  return index;
}
auto LoadNode(std::unordered_map<std::string, unsigned int> &visited, const aiNode &aiNode, const aiScene &aiScene, ModelAsset &model, const std::filesystem::path &path, const std::string &fallbackName, int parent) -> unsigned int {
  const unsigned int index = model.nodes.size();
  model.nodes.emplace_back();
  model.nodes[index].name = aiNode.mName.Empty() ? fallbackName : aiNode.mName.C_Str();
  model.nodes[index].transform = AssimpToGlmMat4(aiNode.mTransformation);
  model.nodes[index].parent = parent;
  for (auto i = 0; i < aiNode.mNumMeshes; ++i) {
    const auto meshId = aiNode.mMeshes[i];
    const auto aiMesh = aiScene.mMeshes[meshId];
    const auto meshIndex = LoadMesh(*aiMesh, model, model.nodes[index].bounds, index, fallbackName);
    model.nodes[index].meshes.push_back(meshIndex);
    auto &mesh = model.meshes.back();
    const auto matId = aiMesh->mMaterialIndex;
    if (matId >= 0 && matId < aiScene.mNumMaterials) {
      const auto aiMaterial = aiScene.mMaterials[matId];
      const auto materialIndex = LoadMaterial(visited, *aiMaterial, model, aiScene, path, fallbackName);
      mesh.material = materialIndex;
    }
  }
  for (auto i = 0; i < aiNode.mNumChildren; ++i) {
    const auto childIndex = LoadNode(visited, *aiNode.mChildren[i], aiScene, model, path, fallbackName, index);
    model.nodes[index].children.push_back(childIndex);
    const auto &childNode = model.nodes[childIndex];
    auto &node = model.nodes[index];
    node.bounds.min = glm::min(node.bounds.min, childNode.bounds.min);
    node.bounds.max = glm::max(node.bounds.max, childNode.bounds.max);
  }
  if (aiNode.mNumMeshes == 0 && aiNode.mNumChildren == 0)
    model.nodes[index].bounds = {.min = glm::vec3(.0f), .max = glm::vec3(.0f)};
  return index;
}
auto ParseAnimations(const aiScene &aiScene, ModelAsset &model) -> void {
  for (auto i = 0; i < aiScene.mNumAnimations; ++i) {
    const auto aiAnim = aiScene.mAnimations[i];
    model.animations.emplace_back();
    auto &clip = model.animations.back();
    clip.name = aiAnim->mName.Empty() ? ("Animation" + std::to_string(i)) : aiAnim->mName.C_Str();
    clip.duration = static_cast<float>(aiAnim->mDuration);
    clip.ticksPerSecond = aiAnim->mTicksPerSecond > 0.0 ? static_cast<float>(aiAnim->mTicksPerSecond) : 25.f;
    for (auto j = 0; j < aiAnim->mNumChannels; ++j) {
      const auto aiChannel = aiAnim->mChannels[j];
      clip.channels.emplace_back();
      auto &channel = clip.channels.back();
      channel.nodeName = aiChannel->mNodeName.C_Str();
      for (auto k = 0; k < aiChannel->mNumPositionKeys; ++k) {
        const auto &key = aiChannel->mPositionKeys[k];
        channel.positions.push_back({.time = static_cast<float>(key.mTime), .value = glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z)});
      }
      for (auto k = 0; k < aiChannel->mNumRotationKeys; ++k) {
        const auto &key = aiChannel->mRotationKeys[k];
        channel.rotations.push_back({.time = static_cast<float>(key.mTime), .value = glm::quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z)});
      }
      for (auto k = 0; k < aiChannel->mNumScalingKeys; ++k) {
        const auto &key = aiChannel->mScalingKeys[k];
        channel.scales.push_back({.time = static_cast<float>(key.mTime), .value = glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z)});
      }
    }
  }
}
auto LoadTexture(std::unordered_map<std::string, unsigned int> &visited, const aiMaterial &aiMaterial, const aiTextureType aiTextureType, ModelMaterial &material, ModelAsset &model, const aiScene &aiScene, const std::filesystem::path &path, const std::string &fallbackName) -> void {
  const auto count = aiMaterial.GetTextureCount(aiTextureType);
  const auto content = AssimpTexToContent(aiTextureType);
  for (auto i = 0; i < count; ++i) {
    const unsigned int index = model.textures.size();
    aiString texPath;
    if (aiMaterial.GetTexture(aiTextureType, i, &texPath) != AI_SUCCESS || texPath.Empty())
      continue;
    const auto embedded = aiScene.GetEmbeddedTexture(texPath.C_Str());
    const auto resolved = embedded ? std::filesystem::path{} : ResolveTexturePath(path, texPath.C_Str());
    const std::string recordedPath = texPath.C_Str();
    const auto cacheKey = embedded ? path.lexically_normal().string() + "#" + recordedPath : (resolved.empty() ? recordedPath : resolved.string());
    if (auto it = visited.find(cacheKey); it != visited.end()) {
      if (it->second == MISSING_TEXTURE)
        continue;
      material.textures.push_back(it->second);
      material.fallback.textureMask.set(static_cast<int>(content));
      continue;
    }
    if (!embedded && resolved.empty()) {
      spdlog::warn("[AssetManager] Texture '{}' is not next to the model or in any texture folder around it, skipping", recordedPath.substr(recordedPath.find_last_of("/\\") + 1));
      visited.insert({cacheKey, MISSING_TEXTURE});
      continue;
    }
    ModelTexture modelTexture{};
    modelTexture.name = embedded ? fallbackName : cacheKey;
    modelTexture.texture.content = content;
    modelTexture.texture.color = DefaultColorSpace(content);
    auto loaded = false;
    if (embedded) {
      if (embedded->mHeight == 0) {
        if (auto *data = stbi_load_from_memory(reinterpret_cast<const unsigned char *>(embedded->pcData), static_cast<int>(embedded->mWidth), &modelTexture.texture.width, &modelTexture.texture.height, &modelTexture.texture.channels, 0); data) {
          const auto size = static_cast<size_t>(modelTexture.texture.width) * modelTexture.texture.height * modelTexture.texture.channels;
          auto &pixels = modelTexture.texture.data.emplace<std::vector<unsigned char>>();
          pixels.assign(data, data + size);
          stbi_image_free(data);
          loaded = true;
        }
      } else
        spdlog::warn("[AssetManager] Embedded texture '{}' uses uncompressed raw texel data, which is not currently supported", texPath.C_Str());
    } else {
      modelTexture.texture.source = cacheKey;
      // takes the baked chain when there is one, which skips decode, mips and compression outright
      loaded = PrepareTextureCached(modelTexture.texture);
    }
    if (!loaded) {
      // remembered as a failure, not left absent: `index` is claimed by whatever loads next, so a
      // key inserted before the load succeeded would send every later reference to another texture
      visited.insert({cacheKey, MISSING_TEXTURE});
      continue;
    }
    if (embedded)
      PrepareTexturePixels(modelTexture.texture);
    visited.insert({cacheKey, index});
    material.textures.push_back(index);
    material.fallback.textureMask.set(static_cast<int>(modelTexture.texture.content));
    // moved, not copied: the pixels are the whole cost of this object, and this is its last use
    model.textures.push_back(std::move(modelTexture));
  }
}
} // namespace kuki
