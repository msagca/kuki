#include "shader.hpp"
#include <fstream>
#include <glad/glad.h>
#include <iostream>
#include <sstream>
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
  progID = glCreateProgram();
  glAttachShader(progID, vShaderID);
  glAttachShader(progID, fShaderID);
  glLinkProgram(progID);
  int success;
  glGetProgramiv(progID, GL_LINK_STATUS, &success);
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
  glUseProgram(progID);
}
void Shader::setBool(const std::string& name, bool value) const {
  glUniform1i(glGetUniformLocation(progID, name.c_str()), (int)value);
}
void Shader::setInt(const std::string& name, int value) const {
  glUniform1i(glGetUniformLocation(progID, name.c_str()), value);
}
void Shader::setFloat(const std::string& name, float value) const {
  glUniform1f(glGetUniformLocation(progID, name.c_str()), value);
}
