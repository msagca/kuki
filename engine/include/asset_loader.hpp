#pragma once
#include <assimp/material.h>
#include <assimp/scene.h>
#include <component/component.hpp>
#include <component/material.hpp>
#include <entity_manager.hpp>
#include <filesystem>
#include <glm/ext/matrix_float4x4.hpp>
#include <kuki_export.h>
#include <primitive.hpp>
#include <string>
#include <unordered_map>
#include <vector>
namespace kuki {
class KUKI_API AssetLoader {
private:
  EntityManager& assetManager;
  std::unordered_map<std::filesystem::path, unsigned int> pathToId;
  glm::mat4 AssimpMatrix4x4ToGlmMat4(const aiMatrix4x4&);
  int LoadNode(aiNode*, const aiScene*, const std::filesystem::path&, int = -1);
  Material CreateMaterial(aiMaterial*, const std::filesystem::path&);
  bool LoadCubeMapSide(unsigned int, const std::filesystem::path&, int);
  Mesh CreateMesh(aiMesh*);
  Mesh CreateMesh(const std::vector<Vertex>&);
  Mesh CreateMesh(const std::vector<Vertex>&, const std::vector<unsigned int>&);
  Mesh CreateVertexBuffer(const std::vector<Vertex>&);
  void CreateIndexBuffer(Mesh&, const std::vector<unsigned int>&);
  void CalculateBounds(Mesh&, const std::vector<Vertex>&);
public:
  AssetLoader(EntityManager&);
  int LoadMesh(const std::string&, const std::vector<Vertex>&);
  int LoadMesh(std::string&, const std::vector<Vertex>&);
  int LoadMesh(std::string&, const std::vector<Vertex>&, const std::vector<unsigned int>&);
  int LoadModel(const std::filesystem::path&);
  int LoadPrimitive(PrimitiveId);
  int LoadTexture(const std::filesystem::path&, TextureType = TextureType::Albedo);
  int LoadCubeMap(std::string&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&);
};
} // namespace kuki
