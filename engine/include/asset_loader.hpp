#pragma once
#include <array>
#include <assimp/material.h>
#include <assimp/scene.h>
#include <bone_data.hpp>
#include <entity_manager.hpp>
#include <event_queue.hpp>
#include <filesystem>
#include <future>
#include <glm/ext/matrix_float4x4.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <primitive.hpp>
#include <string>
#include <vector>
namespace kuki {
struct NodeData {
  std::string name{};
  glm::mat4 transform{1.0f};
  ID parent{};
  Mesh mesh{};
  Material material{};
  BoneData boneData{};
  bool hasMesh{false};
  bool hasMaterial{false};
  bool hasBoneData{false};
};
struct TextureData {
  std::string name{};
  TextureType type{TextureType::Albedo};
  void* data{};
  int width{};
  int height{};
  int channels{1};
};
struct MaterialData {
  std::string name{};
  std::array<TextureData, 7> textureData{};
  glm::vec4 albedo{1.0f};
  glm::vec4 specular{.0f};
  glm::vec4 emissive{.0f};
  float metalness{.5f};
  float occlusion{1.0f};
  float roughness{.5f};
  int textureMask{0};
};
struct PendingNode {
  NodeData nodeData;
  std::promise<ID> nodePromise;
};
struct PendingMaterial {
  MaterialData materialData;
  std::promise<Material> materialPromise;
};
struct PendingMesh {
  aiMesh* meshPtr;
  std::promise<Mesh> meshPromise;
};
struct PendingBoneData {
  aiMesh* meshPtr;
  std::promise<BoneData> boneDataPromise;
};
class Application;
class KUKI_ENGINE_API AssetLoader {
private:
  Application* app;
  EntityManager& assetManager;
  EventQueue<PendingNode> nodeCreateQueue;
  EventQueue<PendingMaterial> materialCreateQueue;
  EventQueue<PendingMesh> meshCreateQueue;
  EventQueue<PendingBoneData> boneDataCreateQueue;
  EventQueue<TextureData> textureLoadQueue;
  /// @brief Convert Assimp's row-major representation to GLM's column-major
  glm::mat4 AssimpToGlmMat4(const aiMatrix4x4&);
  ID CreateMaterialAsset(const MaterialData&);
  ID CreateTextureAsset(const TextureData&);
  ID CreateSkyboxAsset(const TextureData&);
  ID LoadNode(const aiNode*, const aiScene*, const std::filesystem::path&, const std::vector<Material>&, const std::vector<Mesh>&, const std::vector<BoneData>&, const ID = ID::Invalid());
  Material CreateMaterial(const MaterialData&);
  MaterialData LoadMaterial(const aiMaterial*, const std::filesystem::path&);
  void LoadTextureIfExists(const aiMaterial*, aiTextureType, const std::filesystem::path&, MaterialData&, int, TextureType);
  BoneData CreateBoneData(const aiMesh*);
  ID CreateNode(const NodeData&);
  Mesh CreateMesh(const aiMesh*);
  Mesh CreateMesh(const std::vector<Vertex>&, bool = false);
  Mesh CreateMesh(const std::vector<Vertex>&, const std::vector<unsigned int>&, bool = false);
  Mesh CreateVertexBuffer(Mesh&, const std::vector<Vertex>&, bool = false);
  std::future<ID> QueueNodeCreation(const NodeData&);
  std::future<Material> QueueMaterialCreation(const MaterialData&);
  std::future<Mesh> QueueMeshCreation(aiMesh*);
  std::future<BoneData> QueueBoneDataCreation(aiMesh*);
  Texture CreateTexture(const TextureData&);
  TextureData LoadTexture(const std::filesystem::path&, TextureType = TextureType::Albedo);
  void CalculateBounds(Mesh&, const std::vector<Vertex>&);
  void CreateIndexBuffer(Mesh&, const std::vector<unsigned int>&);
public:
  AssetLoader(Application*, EntityManager&);
  /// @brief Process pending load operations
  void Update();
  ID LoadMesh(const std::string&, const std::vector<Vertex>&);
  ID LoadMesh(std::string&, const std::vector<Vertex>&, const std::vector<unsigned int>&);
  ID LoadMesh(std::string&, const std::vector<Vertex>&);
  ID LoadPrimitive(PrimitiveType);
  void LoadModelAsync(std::filesystem::path);
  void LoadTextureAsync(std::filesystem::path, TextureType = TextureType::Albedo);
};
} // namespace kuki
