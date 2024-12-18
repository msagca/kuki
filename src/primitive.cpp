#include <glm/geometric.hpp>
#include <primitive.hpp>
std::vector<float> Primitive::Cube() {
  return {// clang-format off
  // Front face (z = -.5f)
  .5f, -.5f, -.5f, .0f, .0f, -1.0f, -.5f, -.5f, -.5f, .0f, .0f, -1.0f, .5f, .5f, -.5f, .0f, .0f, -1.0f, -.5f, .5f, -.5f, .0f, .0f, -1.0f, .5f, .5f, -.5f, .0f, .0f, -1.0f, -.5f, -.5f, -.5f, .0f, .0f, -1.0f,
  // Back face (z = .5f)
  -.5f, -.5f, .5f, .0f, .0f, 1.0f, .5f, -.5f, .5f, .0f, .0f, 1.0f, .5f, .5f, .5f, .0f, .0f, 1.0f, .5f, .5f, .5f, .0f, .0f, 1.0f, -.5f, .5f, .5f, .0f, .0f, 1.0f, -.5f, -.5f, .5f, .0f, .0f, 1.0f,
  // Left face (x = -.5f)
  -.5f, -.5f, -.5f, -1.0f, .0f, .0f, -.5f, -.5f, .5f, -1.0f, .0f, .0f, -.5f, .5f, .5f, -1.0f, .0f, .0f, -.5f, .5f, .5f, -1.0f, .0f, .0f, -.5f, .5f, -.5f, -1.0f, .0f, .0f, -.5f, -.5f, -.5f, -1.0f, .0f, .0f,
  // Right face (x = .5f)
  .5f, -.5f, .5f, 1.0f, .0f, .0f, .5f, -.5f, -.5f, 1.0f, .0f, .0f, .5f, .5f, .5f, 1.0f, .0f, .0f, .5f, .5f, -.5f, 1.0f, .0f, .0f, .5f, .5f, .5f, 1.0f, .0f, .0f, .5f, -.5f, -.5f, 1.0f, .0f, .0f,
  // Bottom face (y = -.5f)
  -.5f, -.5f, -.5f, .0f, -1.0f, .0f, .5f, -.5f, -.5f, .0f, -1.0f, .0f, .5f, -.5f, .5f, .0f, -1.0f, .0f, .5f, -.5f, .5f, .0f, -1.0f, .0f, -.5f, -.5f, .5f, .0f, -1.0f, .0f, -.5f, -.5f, -.5f, .0f, -1.0f, .0f,
  // Top face (y = .5f)
  .5f, .5f, -.5f, .0f, 1.0f, .0f, -.5f, .5f, -.5f, .0f, 1.0f, .0f, .5f, .5f, .5f, .0f, 1.0f, .0f, -.5f, .5f, .5f, .0f, 1.0f, .0f, .5f, .5f, .5f, .0f, 1.0f, .0f, -.5f, .5f, -.5f, .0f, 1.0f, .0f
  }; // clang-format on
}
std::vector<float> Primitive::Sphere(int level) {
  std::vector<Triangle> triangles = CreateIcosahedron();
  triangles = Subdivide(triangles, level);
  std::vector<float> vertices;
  for (const auto& tri : triangles) {
    glm::vec3 verts[3] = {tri.v1, tri.v2, tri.v3};
    for (const auto& vert : verts) {
      auto norm = glm::normalize(vert);
      vertices.insert(vertices.end(), {vert.x, vert.y, vert.z, norm.x, norm.y, norm.z});
    }
  }
  return vertices;
}
std::vector<Triangle> Primitive::CreateIcosahedron() {
  const float t = (1.0 + std::sqrt(5.0)) / 2.0;
  const float r = 1.0 / std::sqrt(1 + t * t);
  std::vector<glm::vec3> vertices = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
  for (auto& v : vertices)
    v = glm::normalize(v * r);
  return {{vertices[0], vertices[11], vertices[5]}, {vertices[0], vertices[5], vertices[1]}, {vertices[0], vertices[1], vertices[7]}, {vertices[0], vertices[7], vertices[10]}, {vertices[0], vertices[10], vertices[11]}, {vertices[1], vertices[5], vertices[9]}, {vertices[5], vertices[11], vertices[4]}, {vertices[11], vertices[10], vertices[2]}, {vertices[10], vertices[7], vertices[6]}, {vertices[7], vertices[1], vertices[8]}, {vertices[3], vertices[9], vertices[4]}, {vertices[3], vertices[4], vertices[2]}, {vertices[3], vertices[2], vertices[6]}, {vertices[3], vertices[6], vertices[8]}, {vertices[3], vertices[8], vertices[9]}, {vertices[4], vertices[9], vertices[5]}, {vertices[2], vertices[4], vertices[11]}, {vertices[6], vertices[2], vertices[10]}, {vertices[8], vertices[6], vertices[7]}, {vertices[9], vertices[8], vertices[1]}};
}
std::vector<Triangle> Primitive::Subdivide(const std::vector<Triangle>& triangles, int level) {
  std::vector<Triangle> result = triangles;
  for (int i = 0; i < level; ++i) {
    std::vector<Triangle> temp;
    for (const auto& tri : result) {
      glm::vec3 v12 = glm::normalize(tri.v1 + tri.v2);
      glm::vec3 v23 = glm::normalize(tri.v2 + tri.v3);
      glm::vec3 v31 = glm::normalize(tri.v3 + tri.v1);
      temp.push_back({tri.v1, v12, v31});
      temp.push_back({tri.v2, v23, v12});
      temp.push_back({tri.v3, v31, v23});
      temp.push_back({v12, v23, v31});
    }
    result = temp;
  }
  return result;
}
/// <summary>
/// Flip the winding order of triangles in a mesh so that back-face culling works correctly
/// </summary>
void Primitive::FlipWindingOrder(std::vector<float>& vertices, bool withNormals) {
  auto stride = withNormals ? 6 : 3;
  auto strideTri = 3 * stride; // distance between consecutive triangles
  for (auto i = 0; i < vertices.size(); i += strideTri) {
    // swap vertex #0 and vertex #1 of the triangle
    std::swap(vertices[i], vertices[i + stride]);
    std::swap(vertices[i + 1], vertices[i + 1 + stride]);
    std::swap(vertices[i + 2], vertices[i + 2 + stride]);
  }
  if (withNormals)
    for (auto i = 3; i < vertices.size(); i += strideTri) {
      // swap the normals as well
      std::swap(vertices[i], vertices[i + stride]);
      std::swap(vertices[i + 1], vertices[i + 1 + stride]);
      std::swap(vertices[i + 2], vertices[i + 2 + stride]);
    }
}
