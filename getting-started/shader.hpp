#pragma once
#include <glm/ext/matrix_float4x4.hpp>
#include <string>
class Shader {
public:
  unsigned int ID;
  Shader(const char* vShaderPath, const char* fShaderPath);
  void Use() const;
  void SetBool(const std::string& name, bool value) const;
  void SetInt(const std::string& name, int value) const;
  void SetFloat(const std::string& name, float value) const;
  void SetMat4(const std::string& name, glm::mat4 value) const;
};
