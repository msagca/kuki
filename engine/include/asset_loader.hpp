#pragma once
#include <assimp/material.h>
#include <assimp/scene.h>
#include <component/component.hpp>
#include <component/material.hpp>
#include <engine_export.h>
#include <entity_manager.hpp>
#include <filesystem>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <primitive.hpp>
#include <string>
#include <unordered_map>
#include <vector>
class ENGINE_API AssetLoader {
private:
  EntityManager& assetManager;
  std::unordered_map<std::filesystem::path, unsigned int> pathToID;
  int LoadNode(aiNode*, const aiScene*, const std::filesystem::path&, int = -1, const std::string& = "");
  Material CreateMaterial(aiMaterial*, std::string&, const std::filesystem::path&);
  Mesh CreateVertexBuffer(const std::vector<Vertex>&);
  void CreateIndexBuffer(Mesh& mesh, const std::vector<unsigned int>&);
  Mesh CreateMesh(const std::vector<Vertex>&);
  Mesh CreateMesh(const std::vector<Vertex>&, const std::vector<unsigned int>&);
  Mesh CreateMesh(aiMesh*);
  glm::mat4 AssimpMatrix4x4ToGlmMat4(const aiMatrix4x4&);
public:
  AssetLoader(EntityManager&);
  int LoadMaterial(std::string&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&);
  int LoadMesh(std::string&, const std::vector<Vertex>&);
  int LoadMesh(std::string&, const std::vector<Vertex>&, const std::vector<unsigned int>&);
  int LoadModel(std::string&, const std::filesystem::path&);
  int LoadTexture(std::string&, const std::filesystem::path&, TextureType);
};
