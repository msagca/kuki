#pragma once
#include <vector>
#include <component_types.hpp>
class Mesh {
private:
  static MeshFilter CreateVertexBuffer(const std::vector<float>&);
  static void CreateNormalBuffer(MeshFilter&, const std::vector<float>&);
  static void CreateIndexBuffer(MeshFilter&, const std::vector<unsigned int>&);
public:
  static MeshFilter Create(const std::vector<float>&); // input: vertex array
  static MeshFilter Create(const std::vector<float>&, const std::vector<float>&); // input: vertex & normal array
  static MeshFilter Create(const std::vector<float>&, const std::vector<unsigned int>&); // input: vertex & index array
  static MeshFilter Create(const std::vector<float>&, const std::vector<float>&, const std::vector<unsigned int>&); // input: vertex, normal, index array
};
