#pragma once
#include <array>
#include <assimp/material.h>
#include <assimp/scene.h>
#include <component/component.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <entity_manager.hpp>
#include <event_queue.hpp>
#include <filesystem>
#include <glm/ext/matrix_float4x4.hpp>
#include <kuki_engine_export.h>
#include <primitive.hpp>
#include <string>
#include <vector>
#include <future>
namespace kuki {
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
  std::array<TextureData, 5> data{};
};
struct CubeMapData {
  std::string name{};
  std::array<TextureData, 6> data{};
};
struct PendingMaterial {
  MaterialData data;
  std::promise<Material> materialPromise;
};
struct PendingMesh {
  aiMesh* source;
  std::promise<Mesh> meshPromise;
};
class Application;
class KUKI_ENGINE_API AssetLoader {
private:
  Application* app;
  EntityManager& assetManager;
  EventQueue<CubeMapData> cubeMapLoadQueue;
  EventQueue<PendingMaterial*> materialCreateQueue;
  EventQueue<PendingMesh*> meshCreateQueue;
  EventQueue<TextureData> textureLoadQueue;
  glm::mat4 AssimpMatrix4x4ToGlmMat4(const aiMatrix4x4&);
  // NOTE: functions that start with Create* shall be called from the main thread sice OpenGL context is only available on the main thread
  int CreateCubeMapAsset(const CubeMapData&);
  int CreateMaterialAsset(const MaterialData&);
  int CreateTextureAsset(const TextureData&);
  int CreateSkyboxAsset(unsigned int);
  int LoadNode(const aiNode*, const aiScene*, const std::filesystem::path&, const std::vector<Material>&, const std::vector<Mesh>&, int = -1);
  Texture CreateCubeMap(const CubeMapData&);
  Material CreateMaterial(const MaterialData&);
  MaterialData LoadMaterial(const aiMaterial*, const std::filesystem::path&);
  Mesh CreateMesh(const aiMesh*);
  Mesh CreateMesh(const std::vector<Vertex>&);
  Mesh CreateMesh(const std::vector<Vertex>&, const std::vector<unsigned int>&);
  Mesh CreateVertexBuffer(const std::vector<Vertex>&);
  std::future<Material> QueueMaterialCreation(const MaterialData&);
  std::future<Mesh> QueueMeshCreation(aiMesh*);
  Texture CreateTexture(const TextureData&);
  TextureData LoadTexture(const std::filesystem::path&, TextureType = TextureType::Albedo);
  void CalculateBounds(Mesh&, const std::vector<Vertex>&);
  void CreateIndexBuffer(Mesh&, const std::vector<unsigned int>&);
public:
  AssetLoader(Application*, EntityManager&);
  /// @brief Process pending load operations
  void Update();
  int LoadMesh(const std::string&, const std::vector<Vertex>&);
  int LoadMesh(std::string&, const std::vector<Vertex>&, const std::vector<unsigned int>&);
  int LoadMesh(std::string&, const std::vector<Vertex>&);
  int LoadPrimitive(PrimitiveId);
  int LoadCubeMap(const std::string&, const std::array<std::filesystem::path, 6>&);
  void LoadCubeMapAsync(const std::string&, const std::array<std::filesystem::path, 6>&);
  void LoadModelAsync(const std::filesystem::path&);
  void LoadTextureAsync(const std::filesystem::path&, TextureType = TextureType::Albedo);
};
} // namespace kuki
