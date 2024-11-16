#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <string>
class Shader {
public:
  Shader(const char* vShaderPath, const char* fShaderPath);
  unsigned int ID;
  void Use() const;
  void SetBool(const std::string& name, bool value) const;
  void SetInt(const std::string& name, int value) const;
  void SetFloat(const std::string& name, float value) const;
  void SetMat4(const std::string& name, glm::mat4 value) const;
  void SetVec3(const std::string& name, glm::vec3 value) const;
};
