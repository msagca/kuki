#include <array>
#include <cmath>
#include <cstdlib>
#include <glm/ext/scalar_constants.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <primitive.hpp>
#include <utility>
#include <vector>
namespace kuki {
static constexpr auto PI = glm::pi<float>();
auto Primitive::Cube() -> std::vector<Vertex> {
  return {// x, y, z, Nx, Ny, Nz, u, v, Tx, Ty, Tz
    {{.5f, -.5f, -.5f}, {0.f, 0.f, -1.f}, {1.f, 0.f}, {1.f, 0.f, 0.f}},
    {{-.5f, -.5f, -.5f}, {0.f, 0.f, -1.f}, {0.f, 0.f}, {1.f, 0.f, 0.f}},
    {{.5f, .5f, -.5f}, {0.f, 0.f, -1.f}, {1.f, 1.f}, {1.f, 0.f, 0.f}},
    {{-.5f, .5f, -.5f}, {0.f, 0.f, -1.f}, {0.f, 1.f}, {1.f, 0.f, 0.f}},
    {{.5f, .5f, -.5f}, {0.f, 0.f, -1.f}, {1.f, 1.f}, {1.f, 0.f, 0.f}},
    {{-.5f, -.5f, -.5f}, {0.f, 0.f, -1.f}, {0.f, 0.f}, {1.f, 0.f, 0.f}},
    {{-.5f, -.5f, .5f}, {0.f, 0.f, 1.f}, {0.f, 0.f}, {1.f, 0.f, 0.f}},
    {{.5f, -.5f, .5f}, {0.f, 0.f, 1.f}, {1.f, 0.f}, {1.f, 0.f, 0.f}},
    {{.5f, .5f, .5f}, {0.f, 0.f, 1.f}, {1.f, 1.f}, {1.f, 0.f, 0.f}},
    {{.5f, .5f, .5f}, {0.f, 0.f, 1.f}, {1.f, 1.f}, {1.f, 0.f, 0.f}},
    {{-.5f, .5f, .5f}, {0.f, 0.f, 1.f}, {0.f, 1.f}, {1.f, 0.f, 0.f}},
    {{-.5f, -.5f, .5f}, {0.f, 0.f, 1.f}, {0.f, 0.f}, {1.f, 0.f, 0.f}},
    {{-.5f, -.5f, -.5f}, {-1.f, 0.f, 0.f}, {0.f, 0.f}, {0.f, 1.f, 0.f}},
    {{-.5f, -.5f, .5f}, {-1.f, 0.f, 0.f}, {0.f, 1.f}, {0.f, 1.f, 0.f}},
    {{-.5f, .5f, .5f}, {-1.f, 0.f, 0.f}, {1.f, 1.f}, {0.f, 1.f, 0.f}},
    {{-.5f, .5f, .5f}, {-1.f, 0.f, 0.f}, {1.f, 1.f}, {0.f, 1.f, 0.f}},
    {{-.5f, .5f, -.5f}, {-1.f, 0.f, 0.f}, {1.f, 0.f}, {0.f, 1.f, 0.f}},
    {{-.5f, -.5f, -.5f}, {-1.f, 0.f, 0.f}, {0.f, 0.f}, {0.f, 1.f, 0.f}},
    {{.5f, -.5f, .5f}, {1.f, 0.f, 0.f}, {0.f, 1.f}, {0.f, 1.f, 0.f}},
    {{.5f, -.5f, -.5f}, {1.f, 0.f, 0.f}, {0.f, 0.f}, {0.f, 1.f, 0.f}},
    {{.5f, .5f, .5f}, {1.f, 0.f, 0.f}, {1.f, 1.f}, {0.f, 1.f, 0.f}},
    {{.5f, .5f, -.5f}, {1.f, 0.f, 0.f}, {1.f, 0.f}, {0.f, 1.f, 0.f}},
    {{.5f, .5f, .5f}, {1.f, 0.f, 0.f}, {1.f, 1.f}, {0.f, 1.f, 0.f}},
    {{.5f, -.5f, -.5f}, {1.f, 0.f, 0.f}, {0.f, 0.f}, {0.f, 1.f, 0.f}},
    {{-.5f, -.5f, -.5f}, {0.f, -1.f, 0.f}, {0.f, 0.f}, {1.f, 0.f, 0.f}},
    {{.5f, -.5f, -.5f}, {0.f, -1.f, 0.f}, {1.f, 0.f}, {1.f, 0.f, 0.f}},
    {{.5f, -.5f, .5f}, {0.f, -1.f, 0.f}, {1.f, 1.f}, {1.f, 0.f, 0.f}},
    {{.5f, -.5f, .5f}, {0.f, -1.f, 0.f}, {1.f, 1.f}, {1.f, 0.f, 0.f}},
    {{-.5f, -.5f, .5f}, {0.f, -1.f, 0.f}, {0.f, 1.f}, {1.f, 0.f, 0.f}},
    {{-.5f, -.5f, -.5f}, {0.f, -1.f, 0.f}, {0.f, 0.f}, {1.f, 0.f, 0.f}},
    {{.5f, .5f, -.5f}, {0.f, 1.f, 0.f}, {1.f, 0.f}, {1.f, 0.f, 0.f}},
    {{-.5f, .5f, -.5f}, {0.f, 1.f, 0.f}, {0.f, 0.f}, {1.f, 0.f, 0.f}},
    {{.5f, .5f, .5f}, {0.f, 1.f, 0.f}, {1.f, 1.f}, {1.f, 0.f, 0.f}},
    {{-.5f, .5f, .5f}, {0.f, 1.f, 0.f}, {0.f, 1.f}, {1.f, 0.f, 0.f}},
    {{.5f, .5f, .5f}, {0.f, 1.f, 0.f}, {1.f, 1.f}, {1.f, 0.f, 0.f}},
    {{-.5f, .5f, -.5f}, {0.f, 1.f, 0.f}, {0.f, 0.f}, {1.f, 0.f, 0.f}}};
}
auto Primitive::Cylinder(unsigned int segments) -> std::vector<Vertex> {
  std::vector<Vertex> vertices(segments * 36);
  std::vector<float> segmentData(segments * 4);
  const auto circ = 2 * PI / segments;
  for (auto i = 0; i < segments; ++i) { // x, z, Nx, Nz
    const auto angle = circ * i;
    const auto x = cos(angle) * .5f;
    const auto z = sin(angle) * .5f;
    const auto normal = glm::normalize(glm::vec3(x, 0.f, z));
    segmentData[i * 4] = x;
    segmentData[i * 4 + 1] = z;
    segmentData[i * 4 + 2] = normal.x;
    segmentData[i * 4 + 3] = normal.z;
  }
  for (auto i = 0; i < segments; ++i) { // side faces
    const auto iNext = (i + 1) % segments;
    const auto x = segmentData[i * 4];
    const auto z = segmentData[i * 4 + 1];
    const auto nx = segmentData[i * 4 + 2];
    const auto nz = segmentData[i * 4 + 3];
    const auto xNext = segmentData[iNext * 4];
    const auto zNext = segmentData[iNext * 4 + 1];
    const auto nxNext = segmentData[iNext * 4 + 2];
    const auto nzNext = segmentData[iNext * 4 + 3];
    const auto u = static_cast<float>(i) / segments;
    const auto uNext = static_cast<float>(iNext) / segments;
    const glm::vec3 tangent(-nz, 0.f, nx);
    const glm::vec3 tangentNext(-nzNext, 0.f, nxNext);
    vertices.push_back({{x, -.5f, z}, {nx, 0.f, nz}, {u, 0.f}, tangent});
    vertices.push_back({{x, .5f, z}, {nx, 0.f, nz}, {u, 1.f}, tangent});
    vertices.push_back({{xNext, .5f, zNext}, {nxNext, 0.f, nzNext}, {uNext, 1.f}, tangentNext});
    vertices.push_back({{x, -.5f, z}, {nx, 0.f, nz}, {u, 0.f}, tangent});
    vertices.push_back({{xNext, .5f, zNext}, {nxNext, 0.f, nzNext}, {uNext, 1.f}, tangentNext});
    vertices.push_back({{xNext, -.5f, zNext}, {nxNext, 0.f, nzNext}, {uNext, 0.f}, tangentNext});
  }
  for (auto i = 0; i < segments; ++i) { // top cap
    const auto iNext = (i + 1) % segments;
    const auto u = static_cast<float>(i) / segments;
    const auto uNext = static_cast<float>(iNext) / segments;
    const auto angle = circ * i;
    const auto angleNext = circ * iNext;
    const glm::vec3 tangent(-sin(angle), 0.f, cos(angle));
    const glm::vec3 tangentNext(-sin(angleNext), 0.f, cos(angleNext));
    vertices.push_back({{segmentData[i * 4], .5f, segmentData[i * 4 + 1]}, {0.f, 1.f, 0.f}, {u, 0.f}, tangent});
    vertices.push_back({{0.f, .5f, 0.f}, {0.f, 1.f, 0.f}, {.5f, 1.f}, {1.f, 0.f, 0.f}});
    vertices.push_back({{segmentData[iNext * 4], .5f, segmentData[iNext * 4 + 1]}, {0.f, 1.f, 0.f}, {uNext, 0.f}, tangentNext});
  }
  for (auto i = 0; i < segments; ++i) { // bottom cap
    const auto iNext = (i + 1) % segments;
    const auto u = static_cast<float>(i) / segments;
    const auto uNext = static_cast<float>(iNext) / segments;
    const auto angle = circ * i;
    const auto angleNext = circ * iNext;
    const glm::vec3 tangent(sin(angle), 0.f, -cos(angle));
    const glm::vec3 tangentNext(sin(angleNext), 0.f, -cos(angleNext));
    vertices.push_back({{0.f, -.5f, 0.f}, {0.f, -1.f, 0.f}, {.5f, 0.f}, {1.f, 0.f, 0.f}});
    vertices.push_back({{segmentData[i * 4], -.5f, segmentData[i * 4 + 1]}, {0.f, -1.f, 0.f}, {u, 1.f}, tangent});
    vertices.push_back({{segmentData[iNext * 4], -.5f, segmentData[iNext * 4 + 1]}, {0.f, -1.f, 0.f}, {uNext, 1.f}, tangentNext});
  }
  return vertices;
}
auto Primitive::Frame() -> std::vector<Vertex> {
  return {// x, y, z, Nx, Ny, Nz, u, v, Tx, Ty, Tz
    {{-1.f, 1.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, 1.f}, {1.f, 0.f, 0.f}},
    {{-1.f, -1.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, 0.f}, {1.f, 0.f, 0.f}},
    {{1.f, 1.f, 0.f}, {0.f, 0.f, -1.f}, {1.f, 1.f}, {1.f, 0.f, 0.f}},
    {{1.f, 1.f, 0.f}, {0.f, 0.f, -1.f}, {1.f, 1.f}, {1.f, 0.f, 0.f}},
    {{-1.f, -1.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, 0.f}, {1.f, 0.f, 0.f}},
    {{1.f, -1.f, 0.f}, {0.f, 0.f, -1.f}, {1.f, 0.f}, {1.f, 0.f, 0.f}}};
}
auto Primitive::Plane() -> std::vector<Vertex> {
  return {// x, y, z, Nx, Ny, Nz, u, v, Tx, Ty, Tz
    {{-1.f, 0.f, 1.f}, {0.f, 1.f, 0.f}, {0.f, 1.f}, {1.f, 0.f, 0.f}},
    {{1.f, 0.f, 1.f}, {0.f, 1.f, 0.f}, {1.f, 1.f}, {1.f, 0.f, 0.f}},
    {{-1.f, 0.f, -1.f}, {0.f, 1.f, 0.f}, {0.f, 0.f}, {1.f, 0.f, 0.f}},
    {{-1.f, 0.f, -1.f}, {0.f, 1.f, 0.f}, {0.f, 0.f}, {1.f, 0.f, 0.f}},
    {{1.f, 0.f, 1.f}, {0.f, 1.f, 0.f}, {1.f, 1.f}, {1.f, 0.f, 0.f}},
    {{1.f, 0.f, -1.f}, {0.f, 1.f, 0.f}, {1.f, 0.f}, {1.f, 0.f, 0.f}}};
}
auto Primitive::Sphere(unsigned int level) -> std::vector<Vertex> {
  std::vector<Triangle> triangles = CreateIcosahedron();
  Subdivide(triangles, level);
  std::vector<Vertex> vertices(triangles.size() * 18);
  for (const auto &t : triangles) {
    const std::array<glm::vec3, 3> tv{t.v1, t.v2, t.v3};
    for (const auto &v : tv) {
      Vertex vertex{};
      vertex.position = v * .5f;
      vertex.normal = glm::normalize(v);
      const auto theta = atan2(v.z, v.x);
      const auto phi = acos(v.y / glm::length(v));
      vertex.texture = glm::vec2((theta + PI) / (2.f * PI), phi / PI);
      glm::vec3 tangent(-sin(theta), 0.f, cos(theta));
      if (std::abs(v.y) > .999f)
        tangent = glm::vec3(1.f, 0.f, 0.f);
      tangent = glm::normalize(tangent - vertex.normal * glm::dot(tangent, vertex.normal));
      vertex.tangent = tangent;
      vertices.emplace_back(vertex);
    }
  }
  return vertices;
}
auto Primitive::FlipWindingOrder(std::vector<Vertex> &vertices) -> void {
  for (auto i = 1; i < vertices.size(); i += 3)
    // swap vertex #0 and vertex #1 of the triangle
    std::swap(vertices[i - 1], vertices[i]);
}
auto Primitive::CreateIcosahedron() -> std::vector<Triangle> {
  const auto t = (1 + std::sqrt(5.f)) / 2;
  const auto r = 1 / std::sqrt(1 + t * t);
  std::vector<glm::vec3> vertices = {{-1.f, t, 0.f}, {1.f, t, 0.f}, {-1.f, -t, 0.f}, {1.f, -t, 0.f}, {0.f, -1.f, t}, {0.f, 1.f, t}, {0.f, -1.f, -t}, {0.f, 1.f, -t}, {t, 0.f, -1.f}, {t, 0.f, 1.f}, {-t, 0.f, -1.f}, {-t, 0.f, 1.f}};
  for (auto &v : vertices)
    v = glm::normalize(v * r);
  return {{vertices[0], vertices[11], vertices[5]}, {vertices[0], vertices[5], vertices[1]}, {vertices[0], vertices[1], vertices[7]}, {vertices[0], vertices[7], vertices[10]}, {vertices[0], vertices[10], vertices[11]}, {vertices[1], vertices[5], vertices[9]}, {vertices[5], vertices[11], vertices[4]}, {vertices[11], vertices[10], vertices[2]}, {vertices[10], vertices[7], vertices[6]}, {vertices[7], vertices[1], vertices[8]}, {vertices[3], vertices[9], vertices[4]}, {vertices[3], vertices[4], vertices[2]}, {vertices[3], vertices[2], vertices[6]}, {vertices[3], vertices[6], vertices[8]}, {vertices[3], vertices[8], vertices[9]}, {vertices[4], vertices[9], vertices[5]}, {vertices[2], vertices[4], vertices[11]}, {vertices[6], vertices[2], vertices[10]}, {vertices[8], vertices[6], vertices[7]}, {vertices[9], vertices[8], vertices[1]}};
}
auto Primitive::CreateOctahedron() -> std::vector<Triangle> {
  const std::vector<glm::vec3> vertices{{1.f, 0.f, 0.f}, {-1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, -1.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, -1.f}};
  return {{vertices[0], vertices[2], vertices[4]}, {vertices[0], vertices[4], vertices[3]}, {vertices[0], vertices[3], vertices[5]}, {vertices[0], vertices[5], vertices[2]}, {vertices[1], vertices[2], vertices[5]}, {vertices[1], vertices[5], vertices[3]}, {vertices[1], vertices[3], vertices[4]}, {vertices[1], vertices[4], vertices[2]}};
}
auto Primitive::Subdivide(std::vector<Triangle> &triangles, unsigned int level) -> void {
  for (auto i = 0; i < level; ++i) {
    std::vector<Triangle> temp(triangles.size() * 4);
    for (const auto &t : triangles) {
      const auto v12 = glm::normalize(t.v1 + t.v2);
      const auto v23 = glm::normalize(t.v2 + t.v3);
      const auto v31 = glm::normalize(t.v3 + t.v1);
      temp.push_back({t.v1, v12, v31});
      temp.push_back({t.v2, v23, v12});
      temp.push_back({t.v3, v31, v23});
      temp.push_back({v12, v23, v31});
    }
    triangles = temp;
  }
}
} // namespace kuki
