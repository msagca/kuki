#pragma once
#include "glm/geometric.hpp"
#include <primitive.hpp>
#include <glm/vec3.hpp>
std::vector<float> Cube::GetVertices(float size) {
  auto halfSize = size / 2;
  return {-halfSize, -halfSize, -halfSize, halfSize, -halfSize, -halfSize, halfSize, halfSize, -halfSize, -halfSize, halfSize, -halfSize, -halfSize, -halfSize, halfSize, halfSize, -halfSize, halfSize, halfSize, halfSize, halfSize, -halfSize, halfSize, halfSize};
}
std::vector<unsigned int> Cube::GetIndices() {
  return {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 4, 5, 1, 1, 0, 4, 7, 6, 2, 2, 3, 7, 4, 0, 3, 3, 7, 4, 5, 1, 2, 2, 6, 5};
}
std::vector<float> Cube::GetNormals() {
  auto vertices = GetVertices();
  auto indices = GetIndices();
  std::vector<float> normals(vertices.size(), 0.0f);
  for (auto i = 0; i < indices.size(); i += 3) {
    auto i0 = indices[i];
    auto i1 = indices[i + 1];
    auto i2 = indices[i + 2];
    glm::vec3 v0(vertices[i0 * 3], vertices[i0 * 3 + 1], vertices[i0 * 3 + 2]);
    glm::vec3 v1(vertices[i1 * 3], vertices[i1 * 3 + 1], vertices[i1 * 3 + 2]);
    glm::vec3 v2(vertices[i2 * 3], vertices[i2 * 3 + 1], vertices[i2 * 3 + 2]);
    auto edge1 = v1 - v0;
    auto edge2 = v2 - v0;
    auto faceNormal = glm::normalize(glm::cross(edge1, edge2));
    normals[i0 * 3] += faceNormal.x;
    normals[i0 * 3 + 1] += faceNormal.y;
    normals[i0 * 3 + 2] += faceNormal.z;
    normals[i1 * 3] += faceNormal.x;
    normals[i1 * 3 + 1] += faceNormal.y;
    normals[i1 * 3 + 2] += faceNormal.z;
    normals[i2 * 3] += faceNormal.x;
    normals[i2 * 3 + 1] += faceNormal.y;
    normals[i2 * 3 + 2] += faceNormal.z;
  }
  for (auto i = 0; i < vertices.size(); i += 3) {
    glm::vec3 normal(normals[i], normals[i + 1], normals[i + 2]);
    normal = glm::normalize(normal);
    normals[i] = normal.x;
    normals[i + 1] = normal.y;
    normals[i + 2] = normal.z;
  }
  return normals;
}
