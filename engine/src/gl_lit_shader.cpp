#include <camera.hpp>
#include <cstddef>
#include <gl_lit_shader.hpp>
#include <gl_mesh.hpp>
#include <gl_shader.hpp>
#include <gl_skybox.hpp>
#include <glad/glad.h>
#include <light.hpp>
#include <light_type.hpp>
#include <material_fallback.hpp>
#include <span>
namespace kuki {
auto GLLitShader::SetCamera(const Camera &camera, const unsigned int ubo) -> void {
  GLShader::SetCamera(camera, ubo);
  SetUniform("u_viewPos", camera.position);
}
auto GLLitShader::SetLighting() -> void {
  SetUniform("u_pointCount", 0u);
  SetUniform("u_spotCount", 0u);
  SetUniform("u_hasDirLight", false);
}
auto GLLitShader::SetLighting(const Light &light) -> void {
  std::span<const Light> lights(&light, 1);
  SetLighting(lights);
}
auto GLLitShader::SetLighting(std::span<const Light> lights) -> void {
  auto dirExists = false;
  auto pointIndex = 0u;
  auto spotIndex = 0u;
  for (const auto &light : lights)
    if (!dirExists && light.type == LightType::Directional) {
      SetUniform("u_dirLight.direction", light.forward);
      SetUniform("u_dirLight.ambient", light.ambient);
      SetUniform("u_dirLight.diffuse", light.diffuse);
      SetUniform("u_dirLight.specular", light.specular);
      SetUniform("u_dirLight.intensity", light.intensity);
      dirExists = true;
    } else if (light.type == LightType::Point) {
      const auto offset = pointIndex;
      SetUniform(nameToUniform["u_pointLights[0].position"].location + offset, light.position);
      SetUniform(nameToUniform["u_pointLights[0].ambient"].location + offset, light.ambient);
      SetUniform(nameToUniform["u_pointLights[0].diffuse"].location + offset, light.diffuse);
      SetUniform(nameToUniform["u_pointLights[0].specular"].location + offset, light.specular);
      SetUniform(nameToUniform["u_pointLights[0].intensity"].location + offset, light.intensity);
      SetUniform(nameToUniform["u_pointLights[0].constant"].location + offset, light.constant);
      SetUniform(nameToUniform["u_pointLights[0].linear"].location + offset, light.linear);
      SetUniform(nameToUniform["u_pointLights[0].quadratic"].location + offset, light.quadratic);
      ++pointIndex;
    } else if (light.type == LightType::Spot) {
      const auto offset = spotIndex;
      SetUniform(nameToUniform["u_spotLights[0].position"].location + offset, light.position);
      SetUniform(nameToUniform["u_spotLights[0].direction"].location + offset, light.forward);
      SetUniform(nameToUniform["u_spotLights[0].ambient"].location + offset, light.ambient);
      SetUniform(nameToUniform["u_spotLights[0].diffuse"].location + offset, light.diffuse);
      SetUniform(nameToUniform["u_spotLights[0].specular"].location + offset, light.specular);
      SetUniform(nameToUniform["u_spotLights[0].intensity"].location + offset, light.intensity);
      SetUniform(nameToUniform["u_spotLights[0].constant"].location + offset, light.constant);
      SetUniform(nameToUniform["u_spotLights[0].linear"].location + offset, light.linear);
      SetUniform(nameToUniform["u_spotLights[0].quadratic"].location + offset, light.quadratic);
      SetUniform(nameToUniform["u_spotLights[0].innerCutoff"].location + offset, light.innerCutoff);
      SetUniform(nameToUniform["u_spotLights[0].outerCutoff"].location + offset, light.outerCutoff);
      ++spotIndex;
    }
  SetUniform("u_pointCount", pointIndex);
  SetUniform("u_spotCount", spotIndex);
  SetUniform("u_hasDirLight", dirExists);
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
  SetTexture("u_brdfLUT", skybox ? skybox->brdf : 0);
  SetTexture("u_irradianceMap", skybox ? skybox->irradiance : 0);
  SetTexture("u_prefilterMap", skybox ? skybox->prefilter : 0);
  SetUniform("u_hasBRDF", skybox && skybox->brdf > 0);
  SetUniform("u_hasIrradianceMap", skybox && skybox->irradiance > 0);
  SetUniform("u_hasPrefilterMap", skybox && skybox->prefilter > 0);
  SetUniform("u_hasSkybox", skybox != nullptr);
}
} // namespace kuki
