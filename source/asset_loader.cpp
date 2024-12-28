#include <asset_loader.hpp>
#include <asset_manager.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <iostream>
#include <ostream>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <string>
#include <component_types.hpp>
#include <glad/glad.h>
#include <fstream>
#include <sstream>
AssetLoader::AssetLoader(AssetManager& assetManager)
  : assetManager(assetManager) {}
unsigned int AssetLoader::LoadModel(const std::string& name, const std::string& path) {
  Assimp::Importer importer;
  const auto scene = importer.ReadFile(path, aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);
  if (!scene) {
    std::cerr << "Assimp: " << importer.GetErrorString() << std::endl;
    return 0;
  }
  auto assetID = assetManager.Create(name);
  return assetID;
}
static const char* ReadShader(const std::string& filename) {
  std::ifstream shaderFile(filename);
  if (!shaderFile) {
    std::cerr << "Could not open the file: " << filename << "." << std::endl;
    return nullptr;
  }
  std::stringstream shaderStream;
  shaderStream << shaderFile.rdbuf();
  if (shaderFile.fail()) {
    std::cerr << "Failed to read the file: " << filename << "." << std::endl;
    return nullptr;
  }
  shaderFile.close();
  auto shaderStr = shaderStream.str();
  char* shaderCStr = new char[shaderStr.size() + 1];
  strcpy(shaderCStr, shaderStr.c_str());
  return shaderCStr;
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
unsigned int AssetLoader::LoadShader(const std::string& name, const std::string& vertPath, const std::string& fragPath) {
  auto vertText = ReadShader(vertPath);
  auto fragText = ReadShader(fragPath);
  if (!vertText || !fragText)
    return 0;
  GLuint vertID, fragID;
  CompileShader(vertID, vertText, GL_VERTEX_SHADER);
  CompileShader(fragID, fragText, GL_FRAGMENT_SHADER);
  delete[] vertText;
  delete[] fragText;
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
    return 0;
  }
  auto assetID = assetManager.Create(name);
  auto& shader = assetManager.AddComponent<Shader>(assetID);
  shader.id = id;
  return assetID;
}
unsigned int AssetLoader::LoadTexture(const std::string& name, const std::string& path) {
  int width, height, nrComponents;
  auto data = stbi_load(path.c_str(), &width, &height, &nrComponents, 0);
  if (!data) {
    std::cerr << "Failed to load the texture at " << path << "." << std::endl;
    return 0;
  }
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
  auto assetID = assetManager.Create(name);
  auto& texture = assetManager.AddComponent<Texture>(assetID);
  texture.id = id;
  texture.width = width;
  texture.height = height;
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
  auto& mesh = assetManager.AddComponent<Mesh>(assetID);
  mesh = CreateMesh(vertices);
  return assetID;
}
unsigned int AssetLoader::LoadMesh(const std::string& name, const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
  auto assetID = assetManager.Create(name);
  auto& mesh = assetManager.AddComponent<Mesh>(assetID);
  mesh = CreateMesh(vertices, indices);
  return assetID;
}
