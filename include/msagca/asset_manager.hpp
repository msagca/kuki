#pragma once
#include <glad/glad.h>
#include <component_types.hpp>
#include <unordered_map>
#include <string>
#include <vector>
class AssetManager {
public:
  ~AssetManager();
  static AssetManager& GetInstance() {
    static AssetManager instance;
    return instance;
  }
  Mesh CreateMesh(const std::string&, const std::vector<float>&);
  const Mesh& GetMesh(const std::string&) const;
  const std::string& GetMeshName(unsigned int vao) const;
  void DeleteMesh(const std::string&);
private:
  AssetManager() = default;
  AssetManager(const AssetManager&) = delete;
  AssetManager& operator=(const AssetManager&) = delete;
  std::unordered_map<std::string, Mesh> nameMeshMap;
  std::unordered_map<size_t, std::string> meshHashNameMap;
  std::unordered_map<std::string, size_t> meshNameHashMap;
  std::unordered_map<unsigned int, std::string> meshIndexNameMap;
  Mesh CreateVertexBuffer(const std::vector<float>&, bool);
  void CreateIndexBuffer(Mesh&, const std::vector<unsigned int>&);
  std::size_t ComputeMeshHash(const std::vector<float>&) const;
};
