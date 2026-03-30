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
class KUKI_ENGINE_API Primitive {
public:
  static auto Cube() -> std::vector<Vertex>;
  static auto Cylinder(unsigned int = 40) -> std::vector<Vertex>;
  static auto Frame() -> std::vector<Vertex>;
  static auto Plane() -> std::vector<Vertex>;
  static auto Sphere(unsigned int = 4) -> std::vector<Vertex>;
  /// @brief Flip the winding order (clockwise <-> counter-clockwise) of faces in a mesh
  static void FlipWindingOrder(std::vector<Vertex> &);
private:
  static auto CreateIcosahedron() -> std::vector<Triangle>;
  static auto CreateOctahedron() -> std::vector<Triangle>;
  static auto Subdivide(std::vector<Triangle> &, unsigned int = 1) -> void;
};
} // namespace kuki
