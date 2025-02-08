#define GLM_ENABLE_EXPERIMENTAL
#define STB_IMAGE_IMPLEMENTATION
#include <asset_loader.hpp>
#include <asset_manager.hpp>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/matrix4x4.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <component/component.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/shader.hpp>
#include <component/texture.hpp>
#include <component/transform.hpp>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <iosfwd>
#include <iostream>
#include <ostream>
#include <primitive.hpp>
#include <sstream>
#include <stb_image.h>
#include <string>
#include <vector>
AssetLoader::AssetLoader(AssetManager& assetManager)
  : assetManager(assetManager) {}
int AssetLoader::LoadMaterial(const std::string& name, const std::filesystem::path& basePath, const std::filesystem::path& normalPath, const std::filesystem::path& ormPath) {
  auto textureID = LoadTexture(name + "Base", basePath, TextureType::Base);
  auto texture = assetManager.GetComponent<Texture>(textureID);
  if (!texture)
    return -1;
  auto assetID = assetManager.Create(name);
  auto material = assetManager.AddComponent<Material>(assetID);
  material->base = texture->id;
  if (!normalPath.empty()) {
    textureID = LoadTexture(name + "Normal", normalPath, TextureType::Normal);
    texture = assetManager.GetComponent<Texture>(textureID);
    if (texture)
      material->normal = texture->id;
  }
  if (!ormPath.empty()) {
    textureID = LoadTexture(name + "ORM", ormPath, TextureType::ORM);
    texture = assetManager.GetComponent<Texture>(textureID);
    if (texture)
      material->orm = texture->id;
  }
  return assetID;
}
int AssetLoader::LoadTexture(const std::string& name, const std::filesystem::path& path, TextureType type) {
  auto pathStr = path.string();
  auto it = pathToID.find(pathStr);
  if (it != pathToID.end())
    return it->second;
  int width, height, nrComponents;
  auto data = stbi_load(pathStr.c_str(), &width, &height, &nrComponents, 0);
  if (!data) {
    std::cerr << "Failed to load the texture at " << path << "." << std::endl;
    return -1;
  }
  GLint internalFormat;
  GLenum format;
  switch (nrComponents) {
  case 1:
    internalFormat = GL_R8;
    format = GL_RED;
    break;
  case 3:
    internalFormat = (type == TextureType::Base) ? GL_SRGB8 : GL_RGB8;
    format = GL_RGB;
    break;
  case 4:
    internalFormat = (type == TextureType::Base) ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    format = GL_RGBA;
    break;
  default:
    std::cerr << "Unexpected number of components in texture (" << nrComponents << ")." << std::endl;
    stbi_image_free(data);
    return -1;
  }
  unsigned int id;
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);
  glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
  switch (type) {
  case TextureType::Normal:
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    break;
  default:
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }
  stbi_image_free(data);
  auto assetID = assetManager.Create(name);
  auto texture = assetManager.AddComponent<Texture>(assetID);
  texture->id = id;
  texture->type = type;
  pathToID[pathStr] = assetID;
  return assetID;
}
Material AssetLoader::CreateMaterial(aiMaterial* material, const std::string& name, const std::filesystem::path& root) {
  Material mat;
  aiString path;
  if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
    material->GetTexture(aiTextureType_DIFFUSE, 0, &path);
    auto fullPath = root / path.C_Str();
    auto id = LoadTexture(name + "Base", fullPath, TextureType::Base);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      mat.base = texture->id;
  }
  if (material->GetTextureCount(aiTextureType_NORMALS) > 0) {
    material->GetTexture(aiTextureType_NORMALS, 0, &path);
    auto fullPath = root / path.C_Str();
    auto id = LoadTexture(name + "Normal", fullPath, TextureType::Normal);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      mat.normal = texture->id;
  }
  if (material->GetTextureCount(aiTextureType_METALNESS) > 0) {
    material->GetTexture(aiTextureType_METALNESS, 0, &path);
    auto fullPath = root / path.C_Str();
    auto id = LoadTexture(name + "Metalness", fullPath, TextureType::Metalness);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      mat.metalness = texture->id;
  }
  if (material->GetTextureCount(aiTextureType_AMBIENT_OCCLUSION) > 0) {
    material->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &path);
    auto fullPath = root / path.C_Str();
    auto id = LoadTexture(name + "Occlusion", fullPath, TextureType::Occlusion);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      mat.occlusion = texture->id;
  }
  if (material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0) {
    material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &path);
    auto fullPath = root / path.C_Str();
    auto id = LoadTexture(name + "Roughness", fullPath, TextureType::Roughness);
    auto texture = assetManager.GetComponent<Texture>(id);
    if (texture)
      mat.roughness = texture->id;
  }
  return mat;
}
Mesh AssetLoader::CreateMesh(aiMesh* mesh) {
  std::vector<Vertex> vertices;
  for (auto i = 0; i < mesh->mNumVertices; ++i) {
    Vertex vertex{};
    glm::vec3 vector{};
    vector.x = mesh->mVertices[i].x;
    vector.y = mesh->mVertices[i].y;
    vector.z = mesh->mVertices[i].z;
    vertex.position = vector;
    vector.x = mesh->mNormals[i].x;
    vector.y = mesh->mNormals[i].y;
    vector.z = mesh->mNormals[i].z;
    vertex.normal = vector;
    if (mesh->mTextureCoords[0]) {
      glm::vec2 texCoord{};
      texCoord.x = mesh->mTextureCoords[0][i].x;
      texCoord.y = mesh->mTextureCoords[0][i].y;
      vertex.texture = texCoord;
    } else
      vertex.texture = glm::vec2(.0f);
    vertices.push_back(vertex);
  }
  std::vector<unsigned int> indices;
  for (auto i = 0; i < mesh->mNumFaces; ++i) {
    auto& face = mesh->mFaces[i];
    for (auto j = 0; j < face.mNumIndices; ++j)
      indices.push_back(face.mIndices[j]);
  }
  return CreateMesh(vertices, indices);
}
glm::mat4 AssetLoader::AssimpMatrix4x4ToGlmMat4(const aiMatrix4x4& aiMat) {
  return {{aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1}, {aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2}, {aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3}, {aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4}};
}
unsigned int AssetLoader::LoadNode(aiNode* node, const aiScene* scene, const std::filesystem::path& root, int parentID, const std::string& nodeName) {
  auto model = AssimpMatrix4x4ToGlmMat4(node->mTransformation);
  auto name = nodeName.empty() ? node->mName.C_Str() : nodeName;
  auto assetID = assetManager.Create(name);
  auto transform = assetManager.AddComponent<Transform>(assetID);
  glm::vec3 skew;
  glm::vec4 perspective;
  glm::quat orientation;
  glm::decompose(model, transform->scale, orientation, transform->position, skew, perspective);
  transform->position /= transform->scale;
  transform->scale = glm::vec3(1.0f);
  transform->rotation = glm::eulerAngles(orientation);
  transform->parent = parentID;
  for (auto i = 0; i < node->mNumMeshes; ++i) {
    auto mesh = scene->mMeshes[node->mMeshes[i]];
    auto meshComp = assetManager.AddComponent<Mesh>(assetID);
    *meshComp = CreateMesh(mesh);
    if (mesh->mMaterialIndex >= 0) {
      auto material = scene->mMaterials[mesh->mMaterialIndex];
      auto materialComp = assetManager.AddComponent<Material>(assetID);
      *materialComp = CreateMaterial(material, name, root);
    }
  }
  for (auto i = 0; i < node->mNumChildren; ++i) {
    auto childID = LoadNode(node->mChildren[i], scene, root, assetID);
    assetManager.AddChild(assetID, childID);
  }
  return assetID;
}
int AssetLoader::LoadModel(const std::string& name, const std::filesystem::path& path) {
  Assimp::Importer importer;
  const auto scene = importer.ReadFile(path.string(), aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);
  if (!scene || !scene->mRootNode) {
    std::cerr << "Assimp: " << importer.GetErrorString() << std::endl;
    return -1;
  }
  return LoadNode(scene->mRootNode, scene, path.parent_path(), -1, name);
}
std::string AssetLoader::ReadShader(const std::filesystem::path& path) {
  std::ifstream shaderFile(path);
  if (!shaderFile) {
    std::cerr << "Could not open the file: " << path << "." << std::endl;
    return "";
  }
  std::stringstream shaderStream;
  shaderStream << shaderFile.rdbuf();
  if (shaderFile.fail()) {
    std::cerr << "Failed to read the file: " << path << "." << std::endl;
    return "";
  }
  shaderFile.close();
  return shaderStream.str();
}
unsigned int AssetLoader::CompileShader(const char* shaderText, int shaderType) {
  auto shaderID = glCreateShader(shaderType);
  glShaderSource(shaderID, 1, &shaderText, nullptr);
  glCompileShader(shaderID);
  int success;
  glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);
  if (!success)
    std::cerr << "Failed to compile shader." << std::endl;
  return shaderID;
}
int AssetLoader::LoadShader(const std::string& name, const std::filesystem::path& vertPath, const std::filesystem::path& fragPath) {
  auto vertText = ReadShader(vertPath);
  auto fragText = ReadShader(fragPath);
  if (vertText.empty() || fragText.empty())
    return -1;
  auto vertID = CompileShader(vertText.c_str(), GL_VERTEX_SHADER);
  auto fragID = CompileShader(fragText.c_str(), GL_FRAGMENT_SHADER);
  auto id = glCreateProgram();
  glAttachShader(id, vertID);
  glAttachShader(id, fragID);
  glLinkProgram(id);
  int success;
  glGetProgramiv(id, GL_LINK_STATUS, &success);
  glDeleteShader(vertID);
  glDeleteShader(fragID);
  if (!success) {
    std::cerr << "Failed to link the shader program: '" << name << "'." << std::endl;
    return -1;
  }
  auto assetID = assetManager.Create(name);
  auto shader = assetManager.AddComponent<Shader>(assetID);
  shader->id = id;
  return assetID;
}
Mesh AssetLoader::CreateVertexBuffer(const std::vector<Vertex>& vertices) {
  Mesh mesh;
  mesh.vertexCount = vertices.size();
  glGenVertexArrays(1, &mesh.vertexArray);
  glBindVertexArray(mesh.vertexArray);
  glGenBuffers(1, &mesh.vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texture));
  glEnableVertexAttribArray(2);
  return mesh;
}
void AssetLoader::CreateIndexBuffer(Mesh& mesh, const std::vector<unsigned int>& indices) {
  mesh.indexCount = indices.size();
  glBindVertexArray(mesh.vertexArray);
  glGenBuffers(1, &mesh.indexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
}
Mesh AssetLoader::CreateMesh(const std::vector<Vertex>& vertices) {
  auto mesh = CreateVertexBuffer(vertices);
  return mesh;
}
Mesh AssetLoader::CreateMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
  auto mesh = CreateMesh(vertices);
  CreateIndexBuffer(mesh, indices);
  return mesh;
}
unsigned int AssetLoader::LoadMesh(const std::string& name, const std::vector<Vertex>& vertices) {
  auto assetID = assetManager.Create(name);
  auto mesh = assetManager.AddComponent<Mesh>(assetID);
  *mesh = CreateMesh(vertices);
  return assetID;
}
unsigned int AssetLoader::LoadMesh(const std::string& name, const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
  auto assetID = assetManager.Create(name);
  auto mesh = assetManager.AddComponent<Mesh>(assetID);
  *mesh = CreateMesh(vertices, indices);
  return assetID;
}
