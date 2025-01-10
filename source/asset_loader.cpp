#include <asset_loader.hpp>
#include <asset_manager.hpp>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/matrix4x4.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <component_types.hpp>
#include <cstddef>
#include <fstream>
#include <glad/glad.h>
#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iosfwd>
#include <iostream>
#include <limits>
#include <ostream>
#include <primitive.hpp>
#include <sstream>
#include <stb_image.h>
#include <string>
#include <vector>
AssetLoader::AssetLoader(AssetManager& assetManager)
  : assetManager(assetManager) {
  auto id = LoadMesh("Cube", Primitive::Cube());
  assetManager.AddComponent<Transform>(id);
  assetManager.AddComponent<Material>(id);
  id = LoadMesh("Sphere", Primitive::Sphere());
  assetManager.AddComponent<Transform>(id);
  assetManager.AddComponent<Material>(id);
  id = LoadMesh("Cylinder", Primitive::Cylinder());
  assetManager.AddComponent<Transform>(id);
  assetManager.AddComponent<Material>(id);
}
static Texture CreateTexture(const std::string& path, TextureType type) {
  Texture texture;
  int width, height, nrComponents;
  auto data = stbi_load(path.c_str(), &width, &height, &nrComponents, 0);
  if (data) {
    GLenum format = 0;
    if (nrComponents == 1)
      format = GL_RED;
    else if (nrComponents == 3)
      format = GL_RGB;
    else if (nrComponents == 4)
      format = GL_RGBA;
    unsigned int id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    texture.id = id;
    texture.width = width;
    texture.height = height;
    texture.type = type;
  } else
    std::cerr << "Failed to load the texture at " << path << "." << std::endl;
  return texture;
}
static Material CreateMaterial(aiMaterial* material) {
  Material mat;
  for (auto i = 0; i < material->GetTextureCount(aiTextureType_DIFFUSE); ++i) {
    aiString path;
    material->GetTexture(aiTextureType_DIFFUSE, i, &path);
    mat.diffuseMap = CreateTexture(path.C_Str(), TextureType::DiffuseMap);
  }
  for (auto i = 0; i < material->GetTextureCount(aiTextureType_SPECULAR); ++i) {
    aiString path;
    material->GetTexture(aiTextureType_SPECULAR, i, &path);
    mat.specularMap = CreateTexture(path.C_Str(), TextureType::SpecularMap);
  }
  return mat;
}
static Mesh CreateMesh(const std::vector<Vertex>&);
static Mesh CreateMesh(const std::vector<Vertex>&, const std::vector<unsigned int>&);
static Mesh CreateMesh(aiMesh* mesh) {
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
static glm::mat4 AssimpMatrix4x4ToGlmMat4(const aiMatrix4x4&);
static void DecomposeMatrix(const glm::mat4&, glm::vec3&, glm::vec3&, glm::vec3&);
unsigned int AssetLoader::LoadNode(aiNode* node, const aiScene* scene, int parentID) {
  auto assetID = assetManager.Create(node->mName.C_Str());
  auto transform = assetManager.AddComponent<Transform>(assetID);
  transform->parent = parentID;
  auto model = AssimpMatrix4x4ToGlmMat4(node->mTransformation);
  DecomposeMatrix(model, transform->position, transform->scale, transform->rotation);
  for (auto i = 0; i < node->mNumMeshes; ++i) {
    auto mesh = scene->mMeshes[node->mMeshes[i]];
    auto meshComp = assetManager.AddComponent<Mesh>(assetID);
    *meshComp = CreateMesh(mesh);
    if (mesh->mMaterialIndex >= 0) {
      auto material = scene->mMaterials[mesh->mMaterialIndex];
      auto materialComp = assetManager.AddComponent<Material>(assetID);
      *materialComp = CreateMaterial(material);
    }
  }
  for (auto i = 0; i < node->mNumChildren; ++i) {
    auto childID = LoadNode(node->mChildren[i], scene, assetID);
    assetManager.AddChild(assetID, childID);
  }
  return assetID;
}
int AssetLoader::LoadModel(const std::string& name, const std::string& path) {
  Assimp::Importer importer;
  const auto scene = importer.ReadFile(path, aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);
  if (!scene || !scene->mRootNode) {
    std::cerr << "Assimp: " << importer.GetErrorString() << std::endl;
    return -1;
  }
  auto assetID = assetManager.Create(name);
  assetManager.AddComponent<Transform>(assetID);
  auto childID = LoadNode(scene->mRootNode, scene);
  assetManager.AddChild(assetID, childID);
  return assetID;
}
static std::string ReadShader(const std::string& filename) {
  std::ifstream shaderFile(filename);
  if (!shaderFile) {
    std::cerr << "Could not open the file: " << filename << "." << std::endl;
    return "";
  }
  std::stringstream shaderStream;
  shaderStream << shaderFile.rdbuf();
  if (shaderFile.fail()) {
    std::cerr << "Failed to read the file: " << filename << "." << std::endl;
    return "";
  }
  shaderFile.close();
  return shaderStream.str();
}
static void CompileShader(GLuint& shaderID, const char* shaderText, int shaderType) {
  shaderID = glCreateShader(shaderType);
  glShaderSource(shaderID, 1, &shaderText, NULL);
  glCompileShader(shaderID);
  int success;
  glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);
  if (!success)
    std::cerr << "Failed to compile shader." << std::endl;
}
int AssetLoader::LoadShader(const std::string& name, const std::string& vertPath, const std::string& fragPath) {
  auto vertText = ReadShader(vertPath);
  auto fragText = ReadShader(fragPath);
  if (vertText == "" || fragText == "")
    return -1;
  GLuint vertID, fragID;
  CompileShader(vertID, vertText.c_str(), GL_VERTEX_SHADER);
  CompileShader(fragID, fragText.c_str(), GL_FRAGMENT_SHADER);
  auto id = glCreateProgram();
  glAttachShader(id, vertID);
  glAttachShader(id, fragID);
  glLinkProgram(id);
  int success;
  glGetProgramiv(id, GL_LINK_STATUS, &success);
  glDeleteShader(vertID);
  glDeleteShader(fragID);
  if (!success) {
    std::cerr << "Failed to link shader program." << std::endl;
    return -1;
  }
  auto assetID = assetManager.Create(name);
  auto shader = assetManager.AddComponent<Shader>(assetID);
  shader->id = id;
  return assetID;
}
static Mesh CreateVertexBuffer(const std::vector<Vertex>& vertices) {
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
static void CreateIndexBuffer(Mesh& mesh, const std::vector<unsigned int>& indices) {
  mesh.indexCount = indices.size();
  glBindVertexArray(mesh.vertexArray);
  glGenBuffers(1, &mesh.indexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
}
static Mesh CreateMesh(const std::vector<Vertex>& vertices) {
  auto mesh = CreateVertexBuffer(vertices);
  return mesh;
}
static Mesh CreateMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
  auto mesh = CreateMesh(vertices);
  CreateIndexBuffer(mesh, indices);
  return mesh;
}
static BoundingBox CalculateBoundingBox(const std::vector<Vertex>& vertices) {
  BoundingBox box;
  box.min = glm::vec3(std::numeric_limits<float>::max());
  box.max = glm::vec3(std::numeric_limits<float>::lowest());
  for (const auto& v : vertices) {
    box.min = glm::min(box.min, v.position);
    box.max = glm::max(box.max, v.position);
  }
  return box;
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
static void DecomposeMatrix(const glm::mat4& matrix, glm::vec3& position, glm::vec3& scale, glm::vec3& rotation) {
  auto localMatrix = matrix;
  if (glm::epsilonEqual(localMatrix[3][3], .0f, glm::epsilon<float>()))
    return;
  for (auto i = 0; i < 4; ++i)
    for (auto j = 0; j < 4; ++j)
      localMatrix[i][j] /= localMatrix[3][3];
  position = glm::vec3(localMatrix[3]);
  localMatrix[3] = glm::vec4(0, 0, 0, localMatrix[3][3]);
  glm::vec3 row[3]{};
  for (auto i = 0; i < 3; ++i)
    row[i] = localMatrix[i];
  scale.x = glm::length(row[0]);
  scale.y = glm::length(row[1]);
  scale.z = glm::length(row[2]);
  position /= scale;
  if (scale.x != 0)
    row[0] /= scale.x;
  if (scale.y != 0)
    row[1] /= scale.y;
  if (scale.z != 0)
    row[2] /= scale.z;
  glm::mat3 rotationMatrix(row[0], row[1], row[2]);
  rotation = glm::eulerAngles(glm::quat_cast(rotationMatrix));
  scale = glm::vec3(1.0f);
}
static glm::mat4 AssimpMatrix4x4ToGlmMat4(const aiMatrix4x4& aiMat) {
  return {{aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1}, {aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2}, {aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3}, {aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4}};
}
