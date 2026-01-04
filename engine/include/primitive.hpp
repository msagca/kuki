#pragma once
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/ext/vector_int4.hpp>
#include <kuki_engine_export.h>
#include <vector>
namespace kuki {
struct Vertex {
  glm::vec3 position{};
  glm::vec3 normal{};
  glm::vec2 texture{};
  glm::vec3 tangent{};
  glm::ivec4 boneIds{};
  glm::vec4 boneWeights{};
};
struct Triangle {
  glm::vec3 v1{};
  glm::vec3 v2{};
  glm::vec3 v3{};
};
/// @brief A container class for functions that construct primitive shapes
class KUKI_ENGINE_API Primitive {
private:
  static std::vector<Triangle> CreateOctahedron();
  static std::vector<Triangle> CreateIcosahedron();
  static std::vector<Triangle> Subdivide(const std::vector<Triangle> &, unsigned int = 1);
public:
  /// @brief Flip the winding order (clockwise <-> counter-clockwise) of faces in a mesh
  static void FlipWindingOrder(std::vector<Vertex> &);
  static std::vector<Vertex> Cube();
  static std::vector<Vertex> Cylinder(unsigned int = 40);
  static std::vector<Vertex> Frame();
  static std::vector<Vertex> Plane();
  static std::vector<Vertex> Sphere(unsigned int = 4);
};
} // namespace kuki
