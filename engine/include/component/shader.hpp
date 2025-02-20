#pragma once
#include "component/camera.hpp"
#include "component/light.hpp"
#include <engine_export.h>
#include <filesystem>
#include <glad/glad.h>
#include <glm/ext/matrix_float3x3.hpp>
#include <string>
class ENGINE_API Shader {
private:
  GLuint id;
  std::unordered_map<std::string, GLint> locations;
  std::string Read(const std::filesystem::path&);
  GLuint Compile(const char*, GLenum);
  void CacheLocations();
public:
  Shader(const std::filesystem::path&, const std::filesystem::path&);
  GLuint GetID() const;
  void Use() const;
  void SetUniform(const std::string&, const glm::mat4&);
  void SetUniform(const std::string&, const glm::vec3&);
  void SetUniform(const std::string&, float);
  void SetUniform(const std::string&, int);
  void SetUniform(const std::string&, unsigned int);
  void SetUniform(GLint, const glm::mat4&);
  void SetUniform(GLint, const glm::vec3&);
  void SetUniform(GLint, float);
  void SetUniform(GLint, int);
  void SetUniform(GLint, unsigned int);
  void SetMVP(const glm::mat4&, const Camera&);
  void SetLight(const Light*, unsigned int = 0);
};
