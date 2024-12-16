#pragma once
#include <vector>
#include <component_types.hpp>
class Mesh {
private:
  static MeshFilter CreateVertexBuffer(const std::vector<float>&, bool);
  static void CreateIndexBuffer(MeshFilter&, const std::vector<unsigned int>&);
public:
  static MeshFilter Create(const std::vector<float>&);
  static MeshFilter Create(const std::vector<float>&, const std::vector<unsigned int>&);
};
