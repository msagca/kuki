#include <gl_shader.hpp>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <shader_asset.hpp>
#include <spdlog/spdlog.h>
namespace kuki {
GLShader::GLShader()
  : GLShaderBase(std::in_place_type<GLShader>) {}
auto GLShader::DrawInstanced(const GLMesh &mesh, const unsigned int count) -> void {
  if (count == 0)
    return;
  glBindVertexArray(mesh.vao);
  if (mesh.indexCount > 0)
    glDrawElementsInstanced(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0, count);
  else
    glDrawArraysInstanced(GL_TRIANGLES, 0, mesh.vertexCount, count);
  glBindVertexArray(0);
}
auto GLShader::SetBoneTransforms(const BoneData &boneData) -> void {
}
auto GLShader::SetMaterial(const GLMaterial &material) -> void {
  material.Apply(*this);
}
auto GLShader::SetMaterialFallback(const GLMesh &mesh, const MaterialFallback &fallback, const unsigned int buffer) -> void {
  std::span<const MaterialFallback> fallbacks(&fallback, 1);
  SetMaterialFallback(mesh, fallbacks, buffer);
}
auto GLShader::SetTransform(const GLMesh &mesh, const glm::mat4 &transform, const unsigned int buffer) -> void {
  std::span<const glm::mat4> transforms(&transform, 1);
  SetTransform(mesh, transforms, buffer);
}
auto GLShader::SetTransform(const GLMesh &mesh, std::span<const glm::mat4> transforms, const unsigned int buffer) -> void {
  auto bindingIndex = 1;
  glNamedBufferData(buffer, transforms.size() * sizeof(glm::mat4), transforms.data(), GL_DYNAMIC_DRAW);
  glVertexArrayVertexBuffer(mesh.vao, bindingIndex, buffer, 0, sizeof(glm::mat4));
  glVertexArrayBindingDivisor(mesh.vao, bindingIndex, 1);
  auto attribIndex = 4;
  for (auto i = 0; i < 4; ++i) {
    glVertexArrayAttribFormat(mesh.vao, attribIndex, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * i);
    glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
    glEnableVertexArrayAttrib(mesh.vao, attribIndex);
    attribIndex++;
  }
}
auto GLShader::Draw(const GLMesh &mesh) -> void {
  glBindVertexArray(mesh.vao);
  if (mesh.indexCount > 0)
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
  else
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
  glBindVertexArray(0);
}
auto GLShader::SetCamera(const Camera &camera, unsigned int ubo) -> void {
  if (!camera.uboDirty)
    return;
  camera.uboDirty = false;
  auto bindingPoint = 0; // TODO: store binding point in shader
  glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, ubo);
  glNamedBufferSubData(ubo, 0, sizeof(CameraTransform), &camera.transform);
}
auto GLShader::SetLighting(const Light &light) -> void {}
auto GLShader::SetLighting(std::span<const Light> lights) -> void {}
auto GLShader::SetMaterialFallback(const GLMesh &mesh, std::span<const MaterialFallback> fallbacks, const unsigned int buffer) -> void {}
auto GLShader::SetSkybox(const GLSkybox *skybox) -> void {}
} // namespace kuki
