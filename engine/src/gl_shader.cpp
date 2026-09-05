#include <camera.hpp>
#include <gl_material.hpp>
#include <gl_mesh.hpp>
#include <gl_shader.hpp>
#include <gl_skybox.hpp>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float4.hpp>
#include <light.hpp>
#include <material_fallback.hpp>
#include <span>
namespace kuki {
auto GLShader::Draw(const GLMesh &mesh, const unsigned int count) -> void {
  if (count == 0)
    return;
  glBindVertexArray(mesh.vao);
  if (count == 1) {
    if (mesh.indexCount > 0)
      glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
    else
      glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
  } else {
    if (mesh.indexCount > 0)
      glDrawElementsInstanced(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0, count);
    else
      glDrawArraysInstanced(GL_TRIANGLES, 0, mesh.vertexCount, count);
  }
  glBindVertexArray(0);
}
auto GLShader::SetBoneTransforms(std::span<const glm::mat4> matrices, const unsigned int buffer) -> void {
  glNamedBufferData(buffer, matrices.size() * sizeof(glm::mat4), matrices.data(), GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer);
}
auto GLShader::SetEntityIds(const GLMesh &mesh, std::span<const uint32_t> ids, const unsigned int buffer) -> void {
  const auto bindingIndex = 3;
  if (entityIdCount == ids.size())
    glNamedBufferSubData(buffer, 0, entityIdCount * sizeof(uint32_t), ids.data());
  else {
    entityIdCount = ids.size();
    glNamedBufferData(buffer, entityIdCount * sizeof(uint32_t), ids.data(), GL_DYNAMIC_DRAW);
  }
  glVertexArrayVertexBuffer(mesh.vao, bindingIndex, buffer, 0, sizeof(uint32_t));
  glVertexArrayBindingDivisor(mesh.vao, bindingIndex, 1);
  constexpr auto attribIndex = 15;
  glVertexArrayAttribIFormat(mesh.vao, attribIndex, 1, GL_UNSIGNED_INT, 0);
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
}
auto GLShader::SetMaterial(const GLMaterial &material) const -> void {
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
  const auto bindingIndex = 1;
  if (transformCount == transforms.size())
    glNamedBufferSubData(buffer, 0, transformCount * sizeof(glm::mat4), transforms.data());
  else {
    transformCount = transforms.size();
    glNamedBufferData(buffer, transformCount * sizeof(glm::mat4), transforms.data(), GL_DYNAMIC_DRAW);
  }
  glVertexArrayVertexBuffer(mesh.vao, bindingIndex, buffer, 0, sizeof(glm::mat4));
  glVertexArrayBindingDivisor(mesh.vao, bindingIndex, 1);
  auto attribIndex = 4;
  for (auto i = 0; i < 4; ++i) {
    glVertexArrayAttribFormat(mesh.vao, attribIndex, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * i);
    glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
    glEnableVertexArrayAttrib(mesh.vao, attribIndex);
    ++attribIndex;
  }
}
auto GLShader::SetCamera(const Camera &camera, const unsigned int buffer) -> void {
  cameraDirty = camera.dirty;
  constexpr auto bindingPoint = 0;
  glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, buffer);
  glNamedBufferSubData(buffer, 0, sizeof(CameraTransform), &camera.transform);
}
auto GLShader::SetLighting() -> void {}
auto GLShader::SetLighting(const Light &light) -> void {}
auto GLShader::SetLighting(std::span<const Light> lights) -> void {}
auto GLShader::SetMaterialFallback(const GLMesh &mesh, std::span<const MaterialFallback> fallbacks, const unsigned int buffer) -> void {}
auto GLShader::SetSkybox(const GLSkybox *skybox) -> void {}
auto GLShader::SetIndirectLighting(const IndirectLighting &indirect) -> void {}
} // namespace kuki
