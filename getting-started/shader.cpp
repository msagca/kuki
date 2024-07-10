#include "shader.hpp"
#include <cstring>
#include <fstream>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/type_ptr.inl>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <sstream>
#include <string>
static const char* readShader(const char* filename) {
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
  std::strcpy(shaderCStr, shaderStr.c_str());
  return shaderCStr;
}
static void compileShader(unsigned int& shaderID, const char* shaderText, bool isVertex = true) {
  shaderID = glCreateShader(isVertex ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
  glShaderSource(shaderID, 1, &shaderText, NULL);
  glCompileShader(shaderID);
  int success;
  glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);
  if (!success)
    std::cout << "Error: Failed to compile shader." << std::endl;
}
Shader::Shader(const char* vertexPath, const char* fragmentPath) {
  auto vShaderText = readShader(vertexPath);
  auto fShaderText = readShader(fragmentPath);
  unsigned int vShaderID, fShaderID;
  compileShader(vShaderID, vShaderText);
  compileShader(fShaderID, fShaderText, false);
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
void Shader::use() const {
  glUseProgram(ID);
}
void Shader::setBool(const std::string& name, bool value) const {
  auto loc = glGetUniformLocation(ID, name.c_str());
  glUniform1i(loc, (int)value);
}
void Shader::setInt(const std::string& name, int value) const {
  auto loc = glGetUniformLocation(ID, name.c_str());
  glUniform1i(loc, value);
}
void Shader::setFloat(const std::string& name, float value) const {
  auto loc = glGetUniformLocation(ID, name.c_str());
  glUniform1f(loc, value);
}
void Shader::setMat4(const std::string& name, glm::mat4 value) const {
  auto loc = glGetUniformLocation(ID, name.c_str());
  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
}
