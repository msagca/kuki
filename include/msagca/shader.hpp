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
  void Use() const;
  void SetBool(const std::string&, bool) const;
  void SetInt(const std::string&, int) const;
  void SetFloat(const std::string&, float) const;
  void SetMat4(const std::string&, glm::mat4) const;
  void SetVec3(const std::string&, glm::vec3) const;
};
