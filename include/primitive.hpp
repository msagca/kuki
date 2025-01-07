#pragma once
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <vector>
struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 texture;
};
struct Triangle {
  glm::vec3 v1;
  glm::vec3 v2;
  glm::vec3 v3;
};
/// <summary>
/// Contains static functions that return vertex arrays for common primitive shapes.
/// </summary>
class Primitive {
private:
  static std::vector<Triangle> CreateOctahedron();
  static std::vector<Triangle> CreateIcosahedron();
  static std::vector<Triangle> Subdivide(const std::vector<Triangle>&, int);
  static void FlipWindingOrder(std::vector<Vertex>&);
public:
  static std::vector<Vertex> Cube();
  static std::vector<Vertex> Sphere(int = 4);
  static std::vector<Vertex> Cylinder(int = 40);
};
