#include <cmath>
#include <glm/ext/scalar_constants.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <primitive.hpp>
#include <utility>
#include <vector>
const static auto PI = glm::pi<float>();
std::vector<Vertex> Primitive::Cube() {
  return {// x, y, z, Nx, Ny, Nz, u, v
    {{.5f, -.5f, -.5f}, {.0f, .0f, -1.0f}, {.0f, .0f}},
    {{-.5f, -.5f, -.5f}, {.0f, .0f, -1.0f}, {1.0f, .0f}},
    {{.5f, .5f, -.5f}, {.0f, .0f, -1.0f}, {.0f, 1.0f}},
    {{-.5f, .5f, -.5f}, {.0f, .0f, -1.0f}, {1.0f, 1.0f}},
    {{.5f, .5f, -.5f}, {.0f, .0f, -1.0f}, {.0f, 1.0f}},
    {{-.5f, -.5f, -.5f}, {.0f, .0f, -1.0f}, {1.0f, .0f}},
    {{-.5f, -.5f, .5f}, {.0f, .0f, 1.0f}, {.0f, .0f}},
    {{.5f, -.5f, .5f}, {.0f, .0f, 1.0f}, {1.0f, .0f}},
    {{.5f, .5f, .5f}, {.0f, .0f, 1.0f}, {.0f, 1.0f}},
    {{.5f, .5f, .5f}, {.0f, .0f, 1.0f}, {1.0f, 1.0f}},
    {{-.5f, .5f, .5f}, {.0f, .0f, 1.0f}, {.0f, 1.0f}},
    {{-.5f, -.5f, .5f}, {.0f, .0f, 1.0f}, {1.0f, .0f}},
    {{-.5f, -.5f, -.5f}, {-1.0f, .0f, .0f}, {.0f, .0f}},
    {{-.5f, -.5f, .5f}, {-1.0f, .0f, .0f}, {1.0f, .0f}},
    {{-.5f, .5f, .5f}, {-1.0f, .0f, .0f}, {.0f, 1.0f}},
    {{-.5f, .5f, .5f}, {-1.0f, .0f, .0f}, {1.0f, 1.0f}},
    {{-.5f, .5f, -.5f}, {-1.0f, .0f, .0f}, {.0f, 1.0f}},
    {{-.5f, -.5f, -.5f}, {-1.0f, .0f, .0f}, {1.0f, .0f}},
    {{.5f, -.5f, .5f}, {1.0f, .0f, .0f}, {.0f, .0f}},
    {{.5f, -.5f, -.5f}, {1.0f, .0f, .0f}, {1.0f, .0f}},
    {{.5f, .5f, .5f}, {1.0f, .0f, .0f}, {.0f, 1.0f}},
    {{.5f, .5f, -.5f}, {1.0f, .0f, .0f}, {1.0f, 1.0f}},
    {{.5f, .5f, .5f}, {1.0f, .0f, .0f}, {.0f, 1.0f}},
    {{.5f, -.5f, -.5f}, {1.0f, .0f, .0f}, {1.0f, .0f}},
    {{-.5f, -.5f, -.5f}, {.0f, -1.0f, .0f}, {.0f, .0f}},
    {{.5f, -.5f, -.5f}, {.0f, -1.0f, .0f}, {1.0f, .0f}},
    {{.5f, -.5f, .5f}, {.0f, -1.0f, .0f}, {.0f, 1.0f}},
    {{.5f, -.5f, .5f}, {.0f, -1.0f, .0f}, {1.0f, 1.0f}},
    {{-.5f, -.5f, .5f}, {.0f, -1.0f, .0f}, {.0f, 1.0f}},
    {{-.5f, -.5f, -.5f}, {.0f, -1.0f, .0f}, {1.0f, .0f}},
    {{.5f, .5f, -.5f}, {.0f, 1.0f, .0f}, {.0f, .0f}},
    {{-.5f, .5f, -.5f}, {.0f, 1.0f, .0f}, {1.0f, .0f}},
    {{.5f, .5f, .5f}, {.0f, 1.0f, .0f}, {.0f, 1.0f}},
    {{-.5f, .5f, .5f}, {.0f, 1.0f, .0f}, {1.0f, 1.0f}},
    {{.5f, .5f, .5f}, {.0f, 1.0f, .0f}, {.0f, 1.0f}},
    {{-.5f, .5f, -.5f}, {.0f, 1.0f, .0f}, {1.0f, .0f}}};
}
std::vector<Vertex> Primitive::Sphere(int level) {
  std::vector<Triangle> triangles = CreateOctahedron();
  triangles = Subdivide(triangles, level);
  std::vector<Vertex> vertices;
  vertices.reserve(triangles.size() * 18);
  for (const auto& t : triangles) {
    glm::vec3 verts[3] = {t.v1, t.v2, t.v3};
    for (const auto& v : verts) {
      Vertex vertex;
      vertex.position = v * .5f;
      vertex.normal = glm::normalize(v);
      auto theta = atan2(v.z, v.x);
      auto phi = acos(v.y / glm::length(v));
      vertex.texture = glm::vec2((theta + PI) / (2.0f * PI), phi / PI);
      vertices.emplace_back(vertex);
    }
  }
  return vertices;
}
std::vector<Vertex> Primitive::Cylinder(int segments) {
  std::vector<Vertex> vertices;
  vertices.reserve(segments * 36);
  const auto circ = 2 * PI / segments;
  std::vector<float> segmentData(segments * 4);
  // x, z, Nx, Nz
  for (auto i = 0; i < segments; ++i) {
    auto angle = circ * i;
    auto x = cos(angle) * .5f;
    auto z = sin(angle) * .5f;
    glm::vec3 normal = glm::normalize(glm::vec3(x, .0f, z));
    segmentData[i * 4] = x;
    segmentData[i * 4 + 1] = z;
    segmentData[i * 4 + 2] = normal.x;
    segmentData[i * 4 + 3] = normal.z;
  }
  // side faces
  for (auto i = 0; i < segments; ++i) {
    auto iNext = (i + 1) % segments;
    auto x = segmentData[i * 4];
    auto z = segmentData[i * 4 + 1];
    auto nx = segmentData[i * 4 + 2];
    auto nz = segmentData[i * 4 + 3];
    auto xNext = segmentData[iNext * 4];
    auto zNext = segmentData[iNext * 4 + 1];
    auto nxNext = segmentData[iNext * 4 + 2];
    auto nzNext = segmentData[iNext * 4 + 3];
    float u = static_cast<float>(i) / segments;
    float uNext = static_cast<float>(iNext) / segments;
    vertices.push_back({{x, -.5f, z}, {nx, .0f, nz}, {u, 0.0f}});
    vertices.push_back({{x, .5f, z}, {nx, .0f, nz}, {u, 1.0f}});
    vertices.push_back({{xNext, .5f, zNext}, {nxNext, .0f, nzNext}, {uNext, 1.0f}});
    vertices.push_back({{x, -.5f, z}, {nx, .0f, nz}, {u, 0.0f}});
    vertices.push_back({{xNext, .5f, zNext}, {nxNext, .0f, nzNext}, {uNext, 1.0f}});
    vertices.push_back({{xNext, -.5f, zNext}, {nxNext, .0f, nzNext}, {uNext, 0.0f}});
  }
  // top cap
  for (auto i = 0; i < segments; ++i) {
    auto iNext = (i + 1) % segments;
    float u = static_cast<float>(i) / segments;
    float uNext = static_cast<float>(iNext) / segments;
    vertices.push_back({{segmentData[i * 4], .5f, segmentData[i * 4 + 1]}, {.0f, 1.0f, .0f}, {u, 0.0f}});
    vertices.push_back({{.0f, .5f, .0f}, {.0f, 1.0f, .0f}, {0.5f, 1.0f}});
    vertices.push_back({{segmentData[iNext * 4], .5f, segmentData[iNext * 4 + 1]}, {.0f, 1.0f, .0f}, {uNext, 0.0f}});
  }
  // bottom cap
  for (auto i = 0; i < segments; ++i) {
    auto iNext = (i + 1) % segments;
    float u = static_cast<float>(i) / segments;
    float uNext = static_cast<float>(iNext) / segments;
    vertices.push_back({{.0f, -.5f, .0f}, {.0f, -1.0f, .0f}, {0.5f, 0.0f}});
    vertices.push_back({{segmentData[i * 4], -.5f, segmentData[i * 4 + 1]}, {.0f, -1.0f, .0f}, {u, 1.0f}});
    vertices.push_back({{segmentData[iNext * 4], -.5f, segmentData[iNext * 4 + 1]}, {.0f, -1.0f, .0f}, {uNext, 1.0f}});
  }
  return vertices;
}
std::vector<Triangle> Primitive::CreateOctahedron() {
  std::vector<glm::vec3> vertices = {{1.0f, .0f, .0f}, {-1.0f, .0f, .0f}, {.0f, 1.0f, .0f}, {.0f, -1.0f, .0f}, {.0f, .0f, 1.0f}, {.0f, .0f, -1.0f}};
  return {{vertices[0], vertices[2], vertices[4]}, {vertices[0], vertices[4], vertices[3]}, {vertices[0], vertices[3], vertices[5]}, {vertices[0], vertices[5], vertices[2]}, {vertices[1], vertices[2], vertices[5]}, {vertices[1], vertices[5], vertices[3]}, {vertices[1], vertices[3], vertices[4]}, {vertices[1], vertices[4], vertices[2]}};
}
std::vector<Triangle> Primitive::CreateIcosahedron() {
  const auto t = (1 + std::sqrt(5.0f)) / 2;
  const auto r = 1 / std::sqrt(1 + t * t);
  std::vector<glm::vec3> vertices = {{-1.0f, t, .0f}, {1.0f, t, .0f}, {-1.0f, -t, .0f}, {1.0f, -t, .0f}, {.0f, -1.0f, t}, {.0f, 1.0f, t}, {.0f, -1.0f, -t}, {.0f, 1.0f, -t}, {t, .0f, -1.0f}, {t, .0f, 1.0f}, {-t, .0f, -1.0f}, {-t, .0f, 1.0f}};
  for (auto& v : vertices)
    v = glm::normalize(v * r);
  return {{vertices[0], vertices[11], vertices[5]}, {vertices[0], vertices[5], vertices[1]}, {vertices[0], vertices[1], vertices[7]}, {vertices[0], vertices[7], vertices[10]}, {vertices[0], vertices[10], vertices[11]}, {vertices[1], vertices[5], vertices[9]}, {vertices[5], vertices[11], vertices[4]}, {vertices[11], vertices[10], vertices[2]}, {vertices[10], vertices[7], vertices[6]}, {vertices[7], vertices[1], vertices[8]}, {vertices[3], vertices[9], vertices[4]}, {vertices[3], vertices[4], vertices[2]}, {vertices[3], vertices[2], vertices[6]}, {vertices[3], vertices[6], vertices[8]}, {vertices[3], vertices[8], vertices[9]}, {vertices[4], vertices[9], vertices[5]}, {vertices[2], vertices[4], vertices[11]}, {vertices[6], vertices[2], vertices[10]}, {vertices[8], vertices[6], vertices[7]}, {vertices[9], vertices[8], vertices[1]}};
}
std::vector<Triangle> Primitive::Subdivide(const std::vector<Triangle>& triangles, int level) {
  std::vector<Triangle> result = triangles;
  for (auto i = 0; i < level; ++i) {
    std::vector<Triangle> temp;
    for (const auto& t : result) {
      auto v12 = glm::normalize(t.v1 + t.v2);
      auto v23 = glm::normalize(t.v2 + t.v3);
      auto v31 = glm::normalize(t.v3 + t.v1);
      temp.push_back({t.v1, v12, v31});
      temp.push_back({t.v2, v23, v12});
      temp.push_back({t.v3, v31, v23});
      temp.push_back({v12, v23, v31});
    }
    result = temp;
  }
  return result;
}
/// <summary>
/// Flip the winding order (clockwise &lt;--&gt; counter-clockwise) of faces in a mesh
/// </summary>
void Primitive::FlipWindingOrder(std::vector<Vertex>& vertices) {
  for (auto i = 0; i < vertices.size(); i += 3)
    // swap vertex #0 and vertex #1 of the triangle
    std::swap(vertices[i], vertices[i + 1]); // FIXME: it is not guaranteed that the index i+1 is valid
}
