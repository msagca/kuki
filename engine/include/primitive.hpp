#pragma once
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <kuki_export.h>
#include <vector>
namespace kuki {
struct Vertex {
  glm::vec3 position{};
  glm::vec3 normal{};
  glm::vec2 texture{};
  glm::vec3 tangent{};
};
struct Triangle {
  glm::vec3 v1{};
  glm::vec3 v2{};
  glm::vec3 v3{};
};
enum class PrimitiveId : unsigned int {
  Cube,
  Sphere,
  Cylinder,
  Plane,
  CubeInverted,
  Frame
};
/// @brief A container class for functions that construct primitive shapes
class KUKI_API Primitive {
private:
  static std::vector<Triangle> CreateOctahedron();
  static std::vector<Triangle> CreateIcosahedron();
  static std::vector<Triangle> Subdivide(const std::vector<Triangle>&, int);
public:
  /// @brief Flip the winding order (clockwise <-> counter-clockwise) of faces in a mesh
  static void FlipWindingOrder(std::vector<Vertex>&);
  static std::vector<Vertex> Cube();
  static std::vector<Vertex> Cylinder(int = 40);
  static std::vector<Vertex> Frame();
  static std::vector<Vertex> Plane();
  static std::vector<Vertex> Sphere(int = 4);
};
} // namespace kuki
