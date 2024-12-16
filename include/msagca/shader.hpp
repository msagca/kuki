#pragma once
#include <glad/glad.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <string>
class Shader {
public:
  Shader();
  Shader(const char*, const char*);
  GLuint ID;
};
