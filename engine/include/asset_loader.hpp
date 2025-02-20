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
  std::unordered_map<std::string, unsigned int> pathToID;
  unsigned int LoadNode(aiNode*, const aiScene*, const std::filesystem::path&, int = -1, const std::string& = "");
  Material CreateMaterial(aiMaterial*, const std::string&, const std::filesystem::path&);
  Mesh CreateVertexBuffer(const std::vector<Vertex>&);
  void CreateIndexBuffer(Mesh& mesh, const std::vector<unsigned int>&);
  Mesh CreateMesh(const std::vector<Vertex>&);
  Mesh CreateMesh(const std::vector<Vertex>&, const std::vector<unsigned int>&);
  Mesh CreateMesh(aiMesh*);
  glm::mat4 AssimpMatrix4x4ToGlmMat4(const aiMatrix4x4&);
public:
  AssetLoader(EntityManager&);
  // NOTE: int return type is for functions that require a path to the asset, so if the read fails they can return -1
  int LoadModel(const std::string&, const std::filesystem::path&);
  int LoadMaterial(const std::string&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&);
  int LoadTexture(const std::string&, const std::filesystem::path&, TextureType);
  unsigned int LoadMesh(const std::string&, const std::vector<Vertex>&);
  unsigned int LoadMesh(const std::string&, const std::vector<Vertex>&, const std::vector<unsigned int>&);
};
