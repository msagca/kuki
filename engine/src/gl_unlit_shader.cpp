#include <gl_unlit_shader.hpp>
#include <glad/glad.h>
namespace kuki {
auto GLUnlitShader::Draw(const GLMesh &mesh) -> void {
  DrawInstanced(mesh, 1);
}
auto GLUnlitShader::SetMaterialFallback(const GLMesh &mesh, std::span<const MaterialFallback> fallbacks, const unsigned int buffer) -> void {
  const auto bindingIndex = 2;
  auto attribIndex = 8;
  if (materialCount == fallbacks.size())
    glNamedBufferSubData(buffer, 0, materialCount * sizeof(MaterialFallback), fallbacks.data());
  else {
    materialCount = fallbacks.size();
    glNamedBufferData(buffer, materialCount * sizeof(MaterialFallback), fallbacks.data(), GL_DYNAMIC_DRAW);
  }
  glVertexArrayVertexBuffer(mesh.vao, bindingIndex, buffer, 0, sizeof(MaterialFallback));
  glVertexArrayBindingDivisor(mesh.vao, bindingIndex, 1);
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 4, GL_FLOAT, GL_FALSE, offsetof(MaterialFallback, albedo));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  ++attribIndex;
  glVertexArrayAttribIFormat(mesh.vao, attribIndex, 1, GL_INT, offsetof(MaterialFallback, textureMask));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
}
} // namespace kuki
