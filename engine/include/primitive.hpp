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
  /// @brief Multiplied into the material's albedo, so a low-poly model of many colours is one
  /// material, one pool and one draw rather than one of each per colour. White by default, which
  /// leaves a mesh built before vertices had a colour looking as it did. Linear, like the albedo,
  /// and four floats rather than four bytes: eight bits a channel stored linearly band the darks,
  /// and sixteen bytes on a vertex of seventy-six is not what a frame spends its time on.
  ///
  /// Last, so the offsets `probe_trace.hlsl` hardcodes for the normal and texture coordinate stay
  /// put; only the stride moves.
  glm::vec4 color{1.f};
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
  static void FlipWindingOrder(std::vector<Vertex> &);
private:
  static auto CreateIcosahedron() -> std::vector<Triangle>;
  static auto CreateOctahedron() -> std::vector<Triangle>;
  static auto Subdivide(std::vector<Triangle> &, unsigned int = 1) -> void;
};
} // namespace kuki
