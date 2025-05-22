#include <component/component.hpp>
#include <component/camera.hpp>
#include <component/light.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/shader.hpp>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iosfwd>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
namespace kuki {
IShader::IShader(const std::string& name)
  : name(name) {}
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
  int params = 0;
  glGetProgramiv(id, GL_ACTIVE_UNIFORMS, &params);
  const int bufSize = 256;
  char name[bufSize];
  int length;
  int size;
  unsigned int type;
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
ComputeShader::ComputeShader(const std::string& name, const std::filesystem::path& comp, ComputeType type)
  : IShader(name), type(type) {
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
Shader::Shader(const std::string& name, const std::filesystem::path& vert, const std::filesystem::path& frag, MaterialType type)
  : IShader(name), type(type) {
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
  SetUniform("view", camera->view);
  SetUniform("projection", camera->projection);
}
void Shader::SetMaterial(const IMaterial* material) {
  material->Apply(this);
}
void Shader::SetTransform(const Mesh* mesh, glm::mat4 transform, unsigned int buffer) {
  std::vector<glm::mat4> transforms{transform};
  SetTransform(mesh, transforms, buffer);
}
void Shader::SetTransform(const Mesh* mesh, const std::vector<glm::mat4>& transforms, unsigned int buffer) {
  auto bindingIndex = 1;
  glNamedBufferData(buffer, transforms.size() * sizeof(glm::mat4), transforms.data(), GL_DYNAMIC_DRAW);
  glVertexArrayVertexBuffer(mesh->vertexArray, bindingIndex, buffer, 0, sizeof(glm::mat4));
  glVertexArrayBindingDivisor(mesh->vertexArray, bindingIndex, 1);
  auto attribIndex = 4;
  for (auto i = 0; i < 4; ++i) {
    glVertexArrayAttribFormat(mesh->vertexArray, attribIndex, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * i);
    glVertexArrayAttribBinding(mesh->vertexArray, attribIndex, bindingIndex);
    glEnableVertexArrayAttrib(mesh->vertexArray, attribIndex);
    attribIndex++;
  }
}
void Shader::Draw(const Mesh* mesh) {
  glBindVertexArray(mesh->vertexArray);
  if (mesh->indexCount > 0)
    glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
  else
    glDrawArrays(GL_TRIANGLES, 0, mesh->vertexCount);
  glBindVertexArray(0);
}
void Shader::DrawInstanced(const Mesh* mesh, unsigned int count) {
  glBindVertexArray(mesh->vertexArray);
  if (mesh->indexCount > 0)
    glDrawElementsInstanced(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0, count);
  else
    glDrawArraysInstanced(GL_TRIANGLES, 0, mesh->vertexCount, count);
  glBindVertexArray(0);
}
LitShader::LitShader(const std::string& name, const std::filesystem::path& vert, const std::filesystem::path& frag)
  : Shader(name, vert, frag, MaterialType::Lit) {}
void LitShader::SetCamera(const Camera* camera) {
  SetUniform("view", camera->view);
  SetUniform("projection", camera->projection);
  SetUniform("viewPos", camera->position);
}
void LitShader::SetLighting(const std::vector<const Light*>& lights) {
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
void LitShader::SetMaterialFallback(const Mesh* mesh, LitFallbackData material, unsigned int buffer) {
  std::vector<LitFallbackData> materials{material};
  SetMaterialFallback(mesh, materials, buffer);
}
void LitShader::SetMaterialFallback(const Mesh* mesh, const std::vector<LitFallbackData>& materials, unsigned int buffer) {
  auto bindingIndex = 2;
  auto attribIndex = 8;
  glNamedBufferData(buffer, materials.size() * sizeof(LitFallbackData), materials.data(), GL_DYNAMIC_DRAW);
  glVertexArrayVertexBuffer(mesh->vertexArray, bindingIndex, buffer, 0, sizeof(LitFallbackData));
  glVertexArrayBindingDivisor(mesh->vertexArray, bindingIndex, 1);
  glVertexArrayAttribFormat(mesh->vertexArray, attribIndex, 3, GL_FLOAT, GL_FALSE, offsetof(LitFallbackData, albedo));
  glVertexArrayAttribBinding(mesh->vertexArray, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh->vertexArray, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh->vertexArray, attribIndex, 1, GL_FLOAT, GL_FALSE, offsetof(LitFallbackData, metalness));
  glVertexArrayAttribBinding(mesh->vertexArray, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh->vertexArray, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh->vertexArray, attribIndex, 1, GL_FLOAT, GL_FALSE, offsetof(LitFallbackData, occlusion));
  glVertexArrayAttribBinding(mesh->vertexArray, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh->vertexArray, attribIndex);
  attribIndex++;
  glVertexArrayAttribFormat(mesh->vertexArray, attribIndex, 1, GL_FLOAT, GL_FALSE, offsetof(LitFallbackData, roughness));
  glVertexArrayAttribBinding(mesh->vertexArray, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh->vertexArray, attribIndex);
  attribIndex++;
  glVertexArrayAttribIFormat(mesh->vertexArray, attribIndex, 1, GL_INT, offsetof(LitFallbackData, textureMask));
  glVertexArrayAttribBinding(mesh->vertexArray, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh->vertexArray, attribIndex);
}
void LitShader::Draw(const Mesh* mesh) {
  DrawInstanced(mesh, 1);
}
UnlitShader::UnlitShader(const std::string& name, const std::filesystem::path& vert, const std::filesystem::path& frag)
  : Shader(name, vert, frag, MaterialType::Unlit) {}
void UnlitShader::SetMaterialFallback(const Mesh* mesh, UnlitFallbackData material, unsigned int buffer) {
  std::vector<UnlitFallbackData> materials{material};
  SetMaterialFallback(mesh, materials, buffer);
}
void UnlitShader::SetMaterialFallback(const Mesh* mesh, const std::vector<UnlitFallbackData>& materials, unsigned int buffer) {
  auto bindingIndex = 2;
  auto attribIndex = 8;
  glNamedBufferData(buffer, materials.size() * sizeof(UnlitFallbackData), materials.data(), GL_DYNAMIC_DRAW);
  glVertexArrayVertexBuffer(mesh->vertexArray, bindingIndex, buffer, 0, sizeof(UnlitFallbackData));
  glVertexArrayBindingDivisor(mesh->vertexArray, bindingIndex, 1);
  glVertexArrayAttribFormat(mesh->vertexArray, attribIndex, 4, GL_FLOAT, GL_FALSE, offsetof(UnlitFallbackData, base));
  glVertexArrayAttribBinding(mesh->vertexArray, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh->vertexArray, attribIndex);
  attribIndex++;
  glVertexArrayAttribIFormat(mesh->vertexArray, attribIndex, 1, GL_INT, offsetof(UnlitFallbackData, textureMask));
  glVertexArrayAttribBinding(mesh->vertexArray, attribIndex, bindingIndex);
  glEnableVertexArrayAttrib(mesh->vertexArray, attribIndex);
}
void UnlitShader::Draw(const Mesh* mesh) {
  DrawInstanced(mesh, 1);
}
} // namespace kuki
