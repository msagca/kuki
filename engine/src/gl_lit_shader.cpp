#include <gl_lit_shader.hpp>
#include <gl_skybox.hpp>
#include <glad/glad.h>
namespace kuki {
auto GLLitShader::Draw(const GLMesh &mesh) -> void {
  DrawInstanced(mesh, 1);
}
auto GLLitShader::SetCamera(const Camera &camera, const unsigned int ubo) -> void {
  GLShader::SetCamera(camera, ubo);
  SetUniform("viewPos", camera.position);
}
auto GLLitShader::SetLighting() -> void {
  SetUniform("pointCount", 0u);
  SetUniform("hasDirLight", false);
}
auto GLLitShader::SetLighting(const Light &light) -> void {
  std::span<const Light> lights(&light, 1);
  SetLighting(lights);
}
auto GLLitShader::SetLighting(std::span<const Light> lights) -> void {
  auto dirExists = false;
  unsigned int pointIndex = 0;
  for (const auto &light : lights)
    if (light.type == LightType::Directional) {
      SetUniform("dirLight.direction", light.vector);
      SetUniform("dirLight.ambient", light.ambient);
      SetUniform("dirLight.diffuse", light.diffuse);
      SetUniform("dirLight.specular", light.specular);
      dirExists = true;
    } else if (light.type == LightType::Point) {
      const auto offset = pointIndex * 7;
      SetUniform(nameToUniform["pointLights[0].position"].location + offset, light.vector);
      SetUniform(nameToUniform["pointLights[0].ambient"].location + offset, light.ambient);
      SetUniform(nameToUniform["pointLights[0].diffuse"].location + offset, light.diffuse);
      SetUniform(nameToUniform["pointLights[0].specular"].location + offset, light.specular);
      SetUniform(nameToUniform["pointLights[0].constant"].location + offset, light.constant);
      SetUniform(nameToUniform["pointLights[0].linear"].location + offset, light.linear);
      SetUniform(nameToUniform["pointLights[0].quadratic"].location + offset, light.quadratic);
      ++pointIndex;
    }
  SetUniform("pointCount", pointIndex);
  SetUniform("hasDirLight", dirExists);
}
auto GLLitShader::SetMaterialFallback(const GLMesh &mesh, std::span<const MaterialFallback> fallbacks, const unsigned int buffer) -> void {
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
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 4, GL_FLOAT, GL_FALSE, offsetof(MaterialFallback, specular));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  ++attribIndex;
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 4, GL_FLOAT, GL_FALSE, offsetof(MaterialFallback, emissive));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  ++attribIndex;
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 1, GL_FLOAT, GL_FALSE, offsetof(MaterialFallback, metalness));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  ++attribIndex;
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 1, GL_FLOAT, GL_FALSE, offsetof(MaterialFallback, occlusion));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  ++attribIndex;
  glVertexArrayAttribFormat(mesh.vao, attribIndex, 1, GL_FLOAT, GL_FALSE, offsetof(MaterialFallback, roughness));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
  ++attribIndex;
  glVertexArrayAttribIFormat(mesh.vao, attribIndex, 1, GL_INT, offsetof(MaterialFallback, textureMask));
  glVertexArrayAttribBinding(mesh.vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh.vao, attribIndex);
}
auto GLLitShader::SetSkybox(const GLSkybox *skybox) -> void {
  if (!skybox) {
    SetUniform("hasSkybox", false);
    SetUniform("hasIrradianceMap", false);
    SetUniform("hasPrefilterMap", false);
    SetUniform("hasBRDF", false);
    return;
  }
  if (skybox->irradiance > 0)
    SetTexture("irradianceMap", skybox->irradiance);
  if (skybox->prefilter > 0)
    SetTexture("prefilterMap", skybox->prefilter);
  if (skybox->brdf > 0)
    SetTexture("brdfLUT", skybox->brdf);
  SetUniform("hasSkybox", true);
  SetUniform("hasIrradianceMap", skybox->irradiance > 0);
  SetUniform("hasPrefilterMap", skybox->prefilter > 0);
  SetUniform("hasBRDF", skybox->brdf > 0);
}
} // namespace kuki
