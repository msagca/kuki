#include <mesh.hpp>
MeshFilter Mesh::CreateVertexBuffer(const std::vector<float>& vertices, bool withNormals) {
  MeshFilter mesh;
  auto stride = withNormals ? 6 : 3;
  mesh.vertexCount = vertices.size() / stride;
  glGenVertexArrays(1, &mesh.vertexArray);
  glBindVertexArray(mesh.vertexArray);
  glGenBuffers(1, &mesh.vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
  if (withNormals) {
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
  }
  return mesh;
}
void Mesh::CreateIndexBuffer(MeshFilter& mesh, const std::vector<unsigned int>& indices) {
  mesh.indexCount = indices.size();
  glBindVertexArray(mesh.vertexArray);
  glGenBuffers(1, &mesh.indexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
}
MeshFilter Mesh::Create(const std::vector<float>& vertices) {
  return CreateVertexBuffer(vertices, true);
}
MeshFilter Mesh::Create(const std::vector<float>& vertices, const std::vector<unsigned int>& indices) {
  auto mesh = CreateVertexBuffer(vertices, false);
  CreateIndexBuffer(mesh, indices);
  return mesh;
}
