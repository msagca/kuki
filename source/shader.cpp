#include <cstring>
#include <fstream>
#include <glad/glad.h>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <shader.hpp>
#include <sstream>
#include <string>
static const char* ReadShader(const char*);
static void CompileShader(GLuint&, const char*, bool isVertex = true);
Shader::Shader()
  : Shader("default_lit.vert", "default_lit.frag") {}
Shader::Shader(const char* vert, const char* frag) {
  auto vertText = ReadShader(vert);
  auto fragText = ReadShader(frag);
  GLuint vertID, fragID;
  CompileShader(vertID, vertText);
  CompileShader(fragID, fragText, false);
  delete[] vertText;
  delete[] fragText;
  ID = glCreateProgram();
  glAttachShader(ID, vertID);
  glAttachShader(ID, fragID);
  glLinkProgram(ID);
  int success;
  glGetProgramiv(ID, GL_LINK_STATUS, &success);
  if (!success)
    std::cerr << "Failed to link shader program." << std::endl;
  glDeleteShader(vertID);
  glDeleteShader(fragID);
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
    std::cerr << "Failed to read the shader file at " << filename << "." << std::endl;
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
    std::cerr << "Failed to compile shader." << std::endl;
}
