#include <mesh.hpp>
MeshFilter Mesh::CreateVertexBuffer(const std::vector<float>& vertices) {
  MeshFilter mesh;
  mesh.vertexCount = vertices.size() / 3;
  glGenVertexArrays(1, &mesh.vertexArray);
  glBindVertexArray(mesh.vertexArray);
  glGenBuffers(1, &mesh.vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
  return mesh;
}
void Mesh::CreateNormalBuffer(MeshFilter& mesh, const std::vector<float>& normals) {
  glBindVertexArray(mesh.vertexArray);
  glGenBuffers(1, &mesh.normalBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, mesh.normalBuffer);
  glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(float), normals.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(1);
}
void Mesh::CreateIndexBuffer(MeshFilter& mesh, const std::vector<unsigned int>& indices) {
  mesh.indexCount = indices.size();
  glBindVertexArray(mesh.vertexArray);
  glGenBuffers(1, &mesh.indexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
}
MeshFilter Mesh::Create(const std::vector<float>& vertices) {
  return CreateVertexBuffer(vertices);
}
MeshFilter Mesh::Create(const std::vector<float>& vertices, const std::vector<float>& normals) {
  auto mesh = CreateVertexBuffer(vertices);
  CreateNormalBuffer(mesh, normals);
  return mesh;
}
MeshFilter Mesh::Create(const std::vector<float>& vertices, const std::vector<unsigned int>& indices) {
  auto mesh = CreateVertexBuffer(vertices);
  CreateIndexBuffer(mesh, indices);
  return mesh;
}
MeshFilter Mesh::Create(const std::vector<float>& vertices, const std::vector<float>& normals, const std::vector<unsigned int>& indices) {
  auto mesh = CreateVertexBuffer(vertices);
  CreateNormalBuffer(mesh, normals);
  CreateIndexBuffer(mesh, indices);
  return mesh;
}
