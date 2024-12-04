#include <shader.hpp>
#include <fstream>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <sstream>
static const char* ReadShader(const char*);
static void CompileShader(GLuint&, const char*, bool isVertex = true);
Shader::Shader()
  : Shader("default-vert.glsl", "default-frag.glsl") {}
Shader::Shader(const char* vertShaderPath, const char* fragShaderPath) {
  auto vertShaderText = ReadShader(vertShaderPath);
  auto fragShaderText = ReadShader(fragShaderPath);
  GLuint vertShaderID, fragShaderID;
  CompileShader(vertShaderID, vertShaderText);
  CompileShader(fragShaderID, fragShaderText, false);
  delete[] vertShaderText;
  delete[] fragShaderText;
  ID = glCreateProgram();
  glAttachShader(ID, vertShaderID);
  glAttachShader(ID, fragShaderID);
  glLinkProgram(ID);
  int success;
  glGetProgramiv(ID, GL_LINK_STATUS, &success);
  if (!success)
    std::cerr << "Error: Failed to link shader program." << std::endl;
  glDeleteShader(vertShaderID);
  glDeleteShader(fragShaderID);
}
void Shader::Use() const {
  glUseProgram(ID);
}
void Shader::SetBool(const std::string& name, bool value) const {
  auto loc = glGetUniformLocation(ID, name.c_str());
  glUniform1i(loc, (int)value);
}
void Shader::SetInt(const std::string& name, int value) const {
  auto loc = glGetUniformLocation(ID, name.c_str());
  glUniform1i(loc, value);
}
void Shader::SetFloat(const std::string& name, float value) const {
  auto loc = glGetUniformLocation(ID, name.c_str());
  glUniform1f(loc, value);
}
void Shader::SetMat4(const std::string& name, glm::mat4 value) const {
  auto loc = glGetUniformLocation(ID, name.c_str());
  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
}
void Shader::SetVec3(const std::string& name, glm::vec3 value) const {
  auto loc = glGetUniformLocation(ID, name.c_str());
  glUniform3fv(loc, 1, glm::value_ptr(value));
}
static const char* ReadShader(const char* filename) {
  std::ifstream shaderFile;
  shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  std::string shaderStr;
  try {
    shaderFile.open(filename);
    std::stringstream shaderStream;
    shaderStream << shaderFile.rdbuf();
    shaderFile.close();
    shaderStr = shaderStream.str();
  } catch (std::ifstream::failure e) {
    std::cerr << "Error: Failed to read shader file at " << filename << "." << std::endl;
  }
  char* shaderCStr = new char[shaderStr.size() + 1];
  strcpy(shaderCStr, shaderStr.c_str());
  return shaderCStr;
}
static void CompileShader(GLuint& shaderID, const char* shaderText, bool isVertex) {
  shaderID = glCreateShader(isVertex ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
  glShaderSource(shaderID, 1, &shaderText, NULL);
  glCompileShader(shaderID);
  int success;
  glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);
  if (!success)
    std::cerr << "Error: Failed to compile shader." << std::endl;
}
