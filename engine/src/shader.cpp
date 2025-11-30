#include <bone_data.hpp>
#include <buffer_params.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <light.hpp>
#include <material.hpp>
#include <mesh.hpp>
#include <rendering_system.hpp>
#include <shader.hpp>
#include <span>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
namespace kuki {
IShader::IShader(const std::string& name, RenderingSystem& renderer)
  : name(name), renderer(renderer) {}
unsigned int IShader::GetId() const {
  return id;
}
const std::string& IShader::GetName() const {
  return name;
}
std::string IShader::Read(const std::filesystem::path& path) {
  std::ifstream fs(path);
  if (!fs) {
    spdlog::error("Failed to open shader file: '{}'.", path.string());
    return "";
  }
  std::stringstream ss;
  ss << fs.rdbuf();
  if (fs.fail()) {
    spdlog::error("Failed to read shader file: '{}'.", path.string());
    return "";
  }
  fs.close();
  return ss.str();
}
unsigned int IShader::Compile(const char* text, int type) {
  auto id = glCreateShader(type);
  glShaderSource(id, 1, &text, nullptr);
  glCompileShader(id);
  int success;
  glGetShaderiv(id, GL_COMPILE_STATUS, &success);
  if (!success)
    spdlog::error("Failed to compile shader: {}.", name);
  return id;
}
void IShader::CacheLocations() {
  GLint params = 0;
  glGetProgramiv(id, GL_ACTIVE_UNIFORMS, &params);
  constexpr GLuint bufSize = 256;
  GLchar name[bufSize];
  GLsizei length, size;
  GLenum type;
  for (auto i = 0; i < params; ++i) {
    glGetActiveUniform(id, i, bufSize, &length, &size, &type, name);
    locations[name] = glGetUniformLocation(id, name);
  }
}
void IShader::Use() const {
  glUseProgram(id);
}
void IShader::SetUniform(const std::string& name, const glm::mat4& value) {
  glUniformMatrix4fv(locations[name], 1, GL_FALSE, glm::value_ptr(value));
}
void IShader::SetUniform(const std::string& name, const glm::vec3& value) {
  glUniform3fv(locations[name], 1, glm::value_ptr(value));
}
void IShader::SetUniform(const std::string& name, const glm::vec4& value) {
  glUniform4fv(locations[name], 1, glm::value_ptr(value));
}
void IShader::SetUniform(const std::string& name, float value) {
  glUniform1f(locations[name], value);
}
void IShader::SetUniform(const std::string& name, int value) {
  glUniform1i(locations[name], value);
}
void IShader::SetUniform(const std::string& name, unsigned int value) {
  glUniform1ui(locations[name], value);
}
void IShader::SetUniform(int loc, const glm::mat4& value) {
  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
}
void IShader::SetUniform(int loc, const glm::vec3& value) {
  glUniform3fv(loc, 1, glm::value_ptr(value));
}
void IShader::SetUniform(int loc, const glm::vec4& value) {
  glUniform4fv(loc, 1, glm::value_ptr(value));
}
void IShader::SetUniform(int loc, float value) {
  glUniform1f(loc, value);
}
void IShader::SetUniform(int loc, int value) {
  glUniform1i(loc, value);
}
void IShader::SetUniform(int loc, unsigned int value) {
  glUniform1ui(loc, value);
}
ComputeShader::ComputeShader(const std::string& name, const std::filesystem::path& comp, RenderingSystem& renderer, ComputeType type)
  : IShader(name, renderer), type(type) {
  auto compText = Read(comp);
  auto compId = Compile(compText.c_str(), GL_COMPUTE_SHADER);
  id = glCreateProgram();
  glAttachShader(id, compId);
  glLinkProgram(id);
  glDeleteShader(compId);
  int success;
  glGetProgramiv(id, GL_LINK_STATUS, &success);
  if (!success)
    spdlog::error("Failed to link shader: {}.", name);
  else {
    spdlog::info("Shader program is created: {}.", name);
    CacheLocations();
  }
}
ComputeType ComputeShader::GetType() const {
  return type;
}
Shader::Shader(const std::string& name, const std::filesystem::path& vert, const std::filesystem::path& frag, RenderingSystem& renderer, MaterialType type)
  : IShader(name, renderer), type(type) {
  auto vertText = Read(vert);
  auto fragText = Read(frag);
  auto vertId = Compile(vertText.c_str(), GL_VERTEX_SHADER);
  auto fragId = Compile(fragText.c_str(), GL_FRAGMENT_SHADER);
  id = glCreateProgram();
  glAttachShader(id, vertId);
  glAttachShader(id, fragId);
  glLinkProgram(id);
  glDeleteShader(vertId);
  glDeleteShader(fragId);
  int success;
  glGetProgramiv(id, GL_LINK_STATUS, &success);
  if (!success)
    spdlog::error("Failed to link shader: {}.", name);
  else {
    spdlog::info("Shader program is created: {}.", name);
    CacheLocations();
  }
}
MaterialType Shader::GetType() const {
  return type;
}
void Shader::SetCamera(const Camera* camera) {
  if (!camera || !camera->uboDirty)
    return;
  camera->uboDirty = false;
  auto bindingPoint = 0; // TODO: store binding point in shader
  auto bufferParams = BufferParams(sizeof(CameraTransform));
  auto ubo = renderer.uniformBufferPool.Request(bufferParams);
  glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, ubo);
  glNamedBufferSubData(ubo, 0, sizeof(CameraTransform), &camera->transform);
  renderer.uniformBufferPool.Release(bufferParams, ubo);
}
void Shader::SetMaterial(const Material* material) {
  material->Apply(this);
}
void Shader::SetTransform(const Mesh* mesh, const glm::mat4& transform, unsigned int buffer) {
  std::span<const glm::mat4> transforms(&transform, 1);
  SetTransform(mesh, transforms, buffer);
}
void Shader::SetTransform(const Mesh* mesh, std::span<const glm::mat4> transforms, unsigned int buffer) {
  auto bindingIndex = 1;
  glNamedBufferData(buffer, transforms.size() * sizeof(glm::mat4), transforms.data(), GL_DYNAMIC_DRAW);
  glVertexArrayVertexBuffer(mesh->vao, bindingIndex, buffer, 0, sizeof(glm::mat4));
  glVertexArrayBindingDivisor(mesh->vao, bindingIndex, 1);
  auto attribIndex = 4;
  for (auto i = 0; i < 4; ++i) {
    glVertexArrayAttribFormat(mesh->vao, attribIndex, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * i);
    glVertexArrayAttribBinding(mesh->vao, attribIndex, bindingIndex);
    glEnableVertexArrayAttrib(mesh->vao, attribIndex);
    attribIndex++;
  }
}
void Shader::SetBoneTransforms(const BoneData boneData) {
}
void Shader::Draw(const Mesh* mesh) {
  if (!mesh)
    return;
  glBindVertexArray(mesh->vao);
  if (mesh->indexCount > 0)
    glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
  else
    glDrawArrays(GL_TRIANGLES, 0, mesh->vertexCount);
  glBindVertexArray(0);
}
void Shader::DrawInstanced(const Mesh* mesh, unsigned int count) {
  if (!mesh || count == 0)
    return;
  glBindVertexArray(mesh->vao);
  if (mesh->indexCount > 0)
    glDrawElementsInstanced(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0, count);
  else
    glDrawArraysInstanced(GL_TRIANGLES, 0, mesh->vertexCount, count);
  glBindVertexArray(0);
}
LitShader::LitShader(const std::string& name, const std::filesystem::path& vert, const std::filesystem::path& frag, RenderingSystem& renderer)
  : Shader(name, vert, frag, renderer, MaterialType::Lit) {}
void LitShader::SetCamera(const Camera* camera) {
  Shader::SetCamera(camera);
  SetUniform("viewPos", camera->position);
}
void LitShader::SetLighting(const Light* light) {
  std::span<const Light*> lights(&light, 1);
  SetLighting(lights);
}
void LitShader::SetLighting(std::span<const Light*> lights) {
  auto dirExists = false;
  auto pointIndex = 0;
  for (const auto& light : lights)
    if (light->type == LightType::Directional) {
      SetUniform("dirLight.direction", light->vector);
      SetUniform("dirLight.ambient", light->ambient);
      SetUniform("dirLight.diffuse", light->diffuse);
      SetUniform("dirLight.specular", light->specular);
      dirExists = true;
    } else if (light->type == LightType::Point) {
      auto offset = pointIndex * 7;
      SetUniform(locations["pointLights[0].position"] + offset, light->vector);
      SetUniform(locations["pointLights[0].ambient"] + offset, light->ambient);
      SetUniform(locations["pointLights[0].diffuse"] + offset, light->diffuse);
      SetUniform(locations["pointLights[0].specular"] + offset, light->specular);
      SetUniform(locations["pointLights[0].constant"] + offset, light->constant);
      SetUniform(locations["pointLights[0].linear"] + offset, light->linear);
      SetUniform(locations["pointLights[0].quadratic"] + offset, light->quadratic);
      pointIndex++;
    }
  SetUniform("pointCount", pointIndex);
  SetUniform("hasDirLight", dirExists);
}
void LitShader::SetMaterialFallback(const Mesh* mesh, const LitFallbackData& material, unsigned int buffer) {
  std::span<const LitFallbackData> materials(&material, 1);
  SetMaterialFallback(mesh, materials, buffer);
}
void LitShader::SetMaterialFallback(const Mesh* mesh, std::span<const LitFallbackData> materials, unsigned int buffer) {
  auto bindingIndex = 2;
  auto attribIndex = 8;
  glNamedBufferData(buffer, materials.size() * sizeof(LitFallbackData), materials.data(), GL_DYNAMIC_DRAW);
  glVertexArrayVertexBuffer(mesh->vao, bindingIndex, buffer, 0, sizeof(LitFallbackData));
  glVertexArrayBindingDivisor(mesh->vao, bindingIndex, 1);
  glVertexArrayAttribFormat(mesh->vao, attribIndex, 4, GL_FLOAT, GL_FALSE, offsetof(LitFallbackData, albedo));
  glVertexArrayAttribBinding(mesh->vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh->vao, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh->vao, attribIndex, 4, GL_FLOAT, GL_FALSE, offsetof(LitFallbackData, specular));
  glVertexArrayAttribBinding(mesh->vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh->vao, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh->vao, attribIndex, 4, GL_FLOAT, GL_FALSE, offsetof(LitFallbackData, emissive));
  glVertexArrayAttribBinding(mesh->vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh->vao, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh->vao, attribIndex, 1, GL_FLOAT, GL_FALSE, offsetof(LitFallbackData, metalness));
  glVertexArrayAttribBinding(mesh->vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh->vao, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh->vao, attribIndex, 1, GL_FLOAT, GL_FALSE, offsetof(LitFallbackData, occlusion));
  glVertexArrayAttribBinding(mesh->vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh->vao, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh->vao, attribIndex, 1, GL_FLOAT, GL_FALSE, offsetof(LitFallbackData, roughness));
  glVertexArrayAttribBinding(mesh->vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh->vao, attribIndex);
  attribIndex++;
  glVertexArrayAttribIFormat(mesh->vao, attribIndex, 1, GL_INT, offsetof(LitFallbackData, textureMask));
  glVertexArrayAttribBinding(mesh->vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh->vao, attribIndex);
}
void LitShader::Draw(const Mesh* mesh) {
  DrawInstanced(mesh, 1);
}
UnlitShader::UnlitShader(const std::string& name, const std::filesystem::path& vert, const std::filesystem::path& frag, RenderingSystem& renderer)
  : Shader(name, vert, frag, renderer, MaterialType::Unlit) {}
void UnlitShader::SetMaterialFallback(const Mesh* mesh, const UnlitFallbackData& material, unsigned int buffer) {
  std::span<const UnlitFallbackData> materials(&material, 1);
  SetMaterialFallback(mesh, materials, buffer);
}
void UnlitShader::SetMaterialFallback(const Mesh* mesh, std::span<const UnlitFallbackData> materials, unsigned int buffer) {
  auto bindingIndex = 2;
  auto attribIndex = 8;
  glNamedBufferData(buffer, materials.size() * sizeof(UnlitFallbackData), materials.data(), GL_DYNAMIC_DRAW);
  glVertexArrayVertexBuffer(mesh->vao, bindingIndex, buffer, 0, sizeof(UnlitFallbackData));
  glVertexArrayBindingDivisor(mesh->vao, bindingIndex, 1);
  glVertexArrayAttribFormat(mesh->vao, attribIndex, 4, GL_FLOAT, GL_FALSE, offsetof(UnlitFallbackData, base));
  glVertexArrayAttribBinding(mesh->vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh->vao, attribIndex);
  attribIndex++;
  glVertexArrayAttribIFormat(mesh->vao, attribIndex, 1, GL_INT, offsetof(UnlitFallbackData, textureMask));
  glVertexArrayAttribBinding(mesh->vao, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh->vao, attribIndex);
}
void UnlitShader::Draw(const Mesh* mesh) {
  DrawInstanced(mesh, 1);
}
} // namespace kuki
