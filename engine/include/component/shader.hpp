#pragma once
#include "light.hpp"
#include "material.hpp"
#include <filesystem>
#include <glm/ext/matrix_float3x3.hpp>
#include <kuki_export.h>
#include <string>
namespace kuki {
class KUKI_API Shader {
private:
  unsigned int id;
  const std::string name;
  std::string Read(const std::filesystem::path&);
  unsigned int Compile(const char*, unsigned int);
  void CacheLocations();
protected:
  std::unordered_map<std::string, int> locations;
public:
  Shader(const std::string& name, const std::filesystem::path&, const std::filesystem::path&);
  unsigned int GetId() const;
  const std::string& GetName() const;
  void Use() const;
  void SetUniform(const std::string&, const glm::mat4&);
  void SetUniform(const std::string&, const glm::vec3&);
  void SetUniform(const std::string&, const glm::vec4&);
  void SetUniform(const std::string&, float);
  void SetUniform(const std::string&, int);
  void SetUniform(const std::string&, unsigned int);
  void SetUniform(int, const glm::mat4&);
  void SetUniform(int, const glm::vec3&);
  void SetUniform(int, const glm::vec4&);
  void SetUniform(int, float);
  void SetUniform(int, int);
  void SetUniform(int, unsigned int);
};
class KUKI_API LitShader final : public Shader {
public:
  LitShader(const std::string&, const std::filesystem::path&, const std::filesystem::path&);
  void SetLighting(const std::vector<const Light*>&);
  void SetInstanceData(const Mesh*, const std::vector<glm::mat4>&, const std::vector<LitFallbackData>&, unsigned int, unsigned int);
};
class KUKI_API UnlitShader final : public Shader {
public:
  UnlitShader(const std::string&, const std::filesystem::path&, const std::filesystem::path&);
  void SetInstanceData(const Mesh*, const std::vector<glm::mat4>&, const std::vector<UnlitFallbackData>&, unsigned int, unsigned int);
};
} // namespace kuki
