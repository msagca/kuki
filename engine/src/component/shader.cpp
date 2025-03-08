#include <component/component.hpp>
#include <component/light.hpp>
#include <component/shader.hpp>
#include <filesystem>
#include <fstream>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iosfwd>
#include <iostream>
#include <string>
Shader::Shader(std::string name, const std::filesystem::path& vert, const std::filesystem::path& frag)
  : Shader(vert, frag) {
  this->name = name;
}
Shader::Shader(const std::filesystem::path& vert, const std::filesystem::path& frag) {
  auto vertText = Read(vert);
  auto fragText = Read(frag);
  auto vertID = Compile(vertText.c_str(), GL_VERTEX_SHADER);
  auto fragID = Compile(fragText.c_str(), GL_FRAGMENT_SHADER);
  id = glCreateProgram();
  glAttachShader(id, vertID);
  glAttachShader(id, fragID);
  glLinkProgram(id);
  GLint success;
  glGetProgramiv(id, GL_LINK_STATUS, &success);
  if (!success)
    std::cerr << "Failed to link shader." << std::endl;
  else
    CacheLocations();
  glDeleteShader(vertID);
  glDeleteShader(fragID);
}
GLuint Shader::GetID() const {
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
    std::cerr << "Failed to open shader file: '" << path << "'." << std::endl;
    return "";
  }
  std::stringstream ss;
  ss << fs.rdbuf();
  if (fs.fail()) {
    std::cerr << "Failed to read shader file: '" << path << "'." << std::endl;
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
    std::cerr << "Failed to compile shader." << std::endl;
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
void Shader::SetUniform(GLint loc, float value) {
  glUniform1f(loc, value);
}
void Shader::SetUniform(GLint loc, int value) {
  glUniform1i(loc, value);
}
void Shader::SetUniform(GLint loc, unsigned int value) {
  glUniform1i(loc, value);
}
void Shader::SetMVP(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection) {
  SetUniform("model", model);
  SetUniform("view", view);
  SetUniform("projection", projection);
}
void Shader::SetLight(const Light* light, unsigned int index) {
  if (light->type == LightType::Directional) {
    SetUniform("dirLight.direction", light->vector);
    SetUniform("dirLight.ambient", light->ambient);
    SetUniform("dirLight.diffuse", light->diffuse);
    SetUniform("dirLight.specular", light->specular);
  } else {
    auto offset = index * 7;
    SetUniform(locations["pointLights[0].position"] + offset, light->vector);
    SetUniform(locations["pointLights[0].ambient"] + offset, light->ambient);
    SetUniform(locations["pointLights[0].diffuse"] + offset, light->diffuse);
    SetUniform(locations["pointLights[0].specular"] + offset, light->specular);
    SetUniform(locations["pointLights[0].constant"] + offset, light->constant);
    SetUniform(locations["pointLights[0].linear"] + offset, light->linear);
    SetUniform(locations["pointLights[0].quadratic"] + offset, light->quadratic);
  }
}
