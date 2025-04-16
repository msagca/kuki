#include <component/component.hpp>
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
Shader::Shader(const std::string& name, const std::filesystem::path& vert, const std::filesystem::path& frag)
  : name(name) {
  auto vertText = Read(vert);
  auto fragText = Read(frag);
  auto vertId = Compile(vertText.c_str(), GL_VERTEX_SHADER);
  auto fragId = Compile(fragText.c_str(), GL_FRAGMENT_SHADER);
  id = glCreateProgram();
  glAttachShader(id, vertId);
  glAttachShader(id, fragId);
  glLinkProgram(id);
  GLint success;
  glGetProgramiv(id, GL_LINK_STATUS, &success);
  if (!success)
    spdlog::error("Failed to link shader: {}", name);
  else
    CacheLocations();
  glDeleteShader(vertId);
  glDeleteShader(fragId);
}
GLuint Shader::GetId() const {
  return id;
}
const std::string& Shader::GetName() const {
  return name;
}
void Shader::Use() const {
  glUseProgram(id);
}
std::string Shader::Read(const std::filesystem::path& path) {
  std::ifstream fs(path);
  if (!fs) {
    spdlog::error("Failed to open shader file: {}", path.string());
    return "";
  }
  std::stringstream ss;
  ss << fs.rdbuf();
  if (fs.fail()) {
    spdlog::error("Failed to read shader file: {}", path.string());
    return "";
  }
  fs.close();
  return ss.str();
}
GLuint Shader::Compile(const char* text, GLenum type) {
  auto id = glCreateShader(type);
  glShaderSource(id, 1, &text, nullptr);
  glCompileShader(id);
  GLint success;
  glGetShaderiv(id, GL_COMPILE_STATUS, &success);
  if (!success)
    spdlog::error("Failed to compile shader: {}", name);
  return id;
}
void Shader::CacheLocations() {
  GLint params = 0;
  glGetProgramiv(id, GL_ACTIVE_UNIFORMS, &params);
  const GLsizei bufSize = 256;
  GLchar name[bufSize];
  GLsizei length;
  GLint size;
  GLenum type;
  for (auto i = 0; i < params; ++i) {
    glGetActiveUniform(id, i, bufSize, &length, &size, &type, name);
    locations[name] = glGetUniformLocation(id, name);
  }
}
void Shader::SetUniform(const std::string& name, const glm::mat4& value) {
  glUniformMatrix4fv(locations[name], 1, GL_FALSE, glm::value_ptr(value));
}
void Shader::SetUniform(const std::string& name, const glm::vec3& value) {
  glUniform3fv(locations[name], 1, glm::value_ptr(value));
}
void Shader::SetUniform(const std::string& name, const glm::vec4& value) {
  glUniform4fv(locations[name], 1, glm::value_ptr(value));
}
void Shader::SetUniform(const std::string& name, float value) {
  glUniform1f(locations[name], value);
}
void Shader::SetUniform(const std::string& name, unsigned int value) {
  glUniform1i(locations[name], value);
}
void Shader::SetUniform(const std::string& name, int value) {
  glUniform1i(locations[name], value);
}
void Shader::SetUniform(GLint loc, const glm::mat4& value) {
  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
}
void Shader::SetUniform(GLint loc, const glm::vec3& value) {
  glUniform3fv(loc, 1, glm::value_ptr(value));
}
void Shader::SetUniform(GLint loc, const glm::vec4& value) {
  glUniform4fv(loc, 1, glm::value_ptr(value));
}
void Shader::SetUniform(GLint loc, float value) {
  glUniform1f(loc, value);
}
void Shader::SetUniform(GLint loc, int value) {
  glUniform1i(loc, value);
}
void Shader::SetUniform(GLint loc, unsigned int value) {
  glUniform1i(loc, value);
}
void Shader::SetLighting(const std::vector<const Light*>& lights) {
  auto dirExists = false;
  auto index = 0;
  for (const auto& light : lights)
    if (light->type == LightType::Directional) {
      SetUniform("dirLight.direction", light->vector);
      SetUniform("dirLight.ambient", light->ambient);
      SetUniform("dirLight.diffuse", light->diffuse);
      SetUniform("dirLight.specular", light->specular);
      dirExists = true;
    } else {
      auto offset = index * 7;
      SetUniform(locations["pointLights[0].position"] + offset, light->vector);
      SetUniform(locations["pointLights[0].ambient"] + offset, light->ambient);
      SetUniform(locations["pointLights[0].diffuse"] + offset, light->diffuse);
      SetUniform(locations["pointLights[0].specular"] + offset, light->specular);
      SetUniform(locations["pointLights[0].constant"] + offset, light->constant);
      SetUniform(locations["pointLights[0].linear"] + offset, light->linear);
      SetUniform(locations["pointLights[0].quadratic"] + offset, light->quadratic);
      index++;
    }
  SetUniform("pointCount", index);
  SetUniform("dirExists", dirExists);
}
void Shader::SetInstanceData(const Mesh* mesh, const std::vector<glm::mat4>& transforms, const std::vector<LitFallbackData>& materials, unsigned int transformBuffer, unsigned int materialBuffer) {
  auto bindingIndex = 1;
  glNamedBufferData(transformBuffer, transforms.size() * sizeof(glm::mat4), transforms.data(), GL_DYNAMIC_DRAW);
  glVertexArrayVertexBuffer(mesh->vertexArray, bindingIndex, transformBuffer, 0, sizeof(glm::mat4));
  glVertexArrayBindingDivisor(mesh->vertexArray, bindingIndex, 1);
  auto attribIndex = 4;
  for (auto i = 0; i < 4; ++i) {
    glVertexArrayAttribFormat(mesh->vertexArray, attribIndex, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * i);
    glVertexArrayAttribBinding(mesh->vertexArray, attribIndex, bindingIndex);
    glEnableVertexArrayAttrib(mesh->vertexArray, attribIndex);
    attribIndex++;
  }
  bindingIndex++;
  glNamedBufferData(materialBuffer, materials.size() * sizeof(LitFallbackData), materials.data(), GL_DYNAMIC_DRAW);
  glVertexArrayVertexBuffer(mesh->vertexArray, bindingIndex, materialBuffer, 0, sizeof(LitFallbackData));
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
}
} // namespace kuki
