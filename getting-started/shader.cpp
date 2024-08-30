#include "shader.hpp"
#include <cstring>
#include <fstream>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.inl>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <sstream>
#include <string>
static const char* RadShader(const char* filename) {
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
    std::cout << "Error: Failed to read shader file." << std::endl;
  }
  char* shaderCStr = new char[shaderStr.size() + 1];
  strcpy_s(shaderCStr, shaderStr.size() + 1, shaderStr.c_str());
  return shaderCStr;
}
static void CompileShader(unsigned int& shaderID, const char* shaderText, bool isVertex = true) {
  shaderID = glCreateShader(isVertex ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
  glShaderSource(shaderID, 1, &shaderText, NULL);
  glCompileShader(shaderID);
  int success;
  glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);
  if (!success)
    std::cout << "Error: Failed to compile shader." << std::endl;
}
Shader::Shader(const char* vertexPath, const char* fragmentPath) {
  auto vShaderText = RadShader(vertexPath);
  auto fShaderText = RadShader(fragmentPath);
  unsigned int vShaderID, fShaderID;
  CompileShader(vShaderID, vShaderText);
  CompileShader(fShaderID, fShaderText, false);
  ID = glCreateProgram();
  glAttachShader(ID, vShaderID);
  glAttachShader(ID, fShaderID);
  glLinkProgram(ID);
  int success;
  glGetProgramiv(ID, GL_LINK_STATUS, &success);
  if (!success)
    std::cout << "Error: Failed to link program." << std::endl;
  delete[] vShaderText;
  delete[] fShaderText;
  vShaderText = nullptr;
  fShaderText = nullptr;
  glDeleteShader(vShaderID);
  glDeleteShader(fShaderID);
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
