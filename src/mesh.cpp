#include <mesh.hpp>
MeshFilter CreateMesh(const std::vector<float>& vertices) {
  MeshFilter mesh;
  mesh.vertexCount = vertices.size() / 3;
  glGenVertexArrays(1, &mesh.vao);
  glBindVertexArray(mesh.vao);
  glGenBuffers(1, &mesh.vbo);
  glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
  glBindVertexArray(0);
  return mesh;
}
MeshFilter CreateMesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices) {
  auto mesh = CreateMesh(vertices);
  mesh.indexCount = indices.size();
  glBindVertexArray(mesh.vao);
  glGenBuffers(1, &mesh.ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
  glBindVertexArray(0);
  return mesh;
}
