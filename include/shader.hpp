#pragma once
#include <glad/glad.h>
class Shader {
public:
  Shader();
  Shader(const char*, const char*);
  GLuint ID;
};
