#ifndef SHADER_H
#define SHADER_H
#include <glm/ext/matrix_float4x4.hpp>
#include <string>
class Shader {
public:
  unsigned int ID;
  Shader(const char* vertexPath, const char* fragmentPath);
  void use() const;
  void setBool(const std::string& name, bool value) const;
  void setInt(const std::string& name, int value) const;
  void setFloat(const std::string& name, float value) const;
  void setMat4(const std::string& name, glm::mat4 value) const;
};
#endif
