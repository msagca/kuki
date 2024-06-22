#include "shader.hpp"
#include <fstream>
#include <glad/glad.h>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <sstream>
#include <string>
Shader::Shader(const char* vertexPath, const char* fragmentPath) {
  string vShaderStr;
  string fShaderStr;
  ifstream vShaderFile;
  ifstream fShaderFile;
  vShaderFile.exceptions(ifstream::failbit | ifstream::badbit);
  fShaderFile.exceptions(ifstream::failbit | ifstream::badbit);
  try {
    vShaderFile.open(vertexPath);
    fShaderFile.open(fragmentPath);
    stringstream vShaderStream, fShaderStream;
    vShaderStream << vShaderFile.rdbuf();
    fShaderStream << fShaderFile.rdbuf();
    vShaderFile.close();
    fShaderFile.close();
    vShaderStr = vShaderStream.str();
    fShaderStr = fShaderStream.str();
  } catch (ifstream::failure e) {
    cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << endl;
  }
  const char* vShaderCode = vShaderStr.c_str();
  const char* fShaderCode = fShaderStr.c_str();
  unsigned int vShaderID, fShaderID;
  int success;
  char infoLog[512];
  vShaderID = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vShaderID, 1, &vShaderCode, NULL);
  glCompileShader(vShaderID);
  glGetShaderiv(vShaderID, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(vShaderID, 512, NULL, infoLog);
    cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
         << infoLog << endl;
  };
  fShaderID = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fShaderID, 1, &fShaderCode, NULL);
  glCompileShader(fShaderID);
  glGetShaderiv(fShaderID, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(fShaderID, 512, NULL, infoLog);
    cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
         << infoLog << endl;
  };
  progID = glCreateProgram();
  glAttachShader(progID, vShaderID);
  glAttachShader(progID, fShaderID);
  glLinkProgram(progID);
  glGetProgramiv(progID, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(progID, 512, NULL, infoLog);
    cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
         << infoLog << endl;
  }
  glDeleteShader(vShaderID);
  glDeleteShader(fShaderID);
}
void Shader::use() const {
  glUseProgram(progID);
}
void Shader::setBool(const string& name, bool value) const {
  glUniform1i(glGetUniformLocation(progID, name.c_str()), (int)value);
}
void Shader::setInt(const string& name, int value) const {
  glUniform1i(glGetUniformLocation(progID, name.c_str()), value);
}
void Shader::setFloat(const string& name, float value) const {
  glUniform1f(glGetUniformLocation(progID, name.c_str()), value);
}
