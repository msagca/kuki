#pragma once
#include "component.hpp"
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
  MaterialType type;
  std::string Read(const std::filesystem::path&);
  unsigned int Compile(const char*, unsigned int);
  void CacheLocations();
protected:
  std::unordered_map<std::string, int> locations;
public:
  Shader(const std::string&, const std::filesystem::path&, const std::filesystem::path&, MaterialType = MaterialType::Unlit);
  unsigned int GetId() const;
  const std::string& GetName() const;
  MaterialType GetType() const;
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
  virtual void SetCamera(const Camera*);
  void SetMaterial(const IMaterial*);
  /// @brief Set transform attributes for a single instance
  void SetTransform(const Mesh*, glm::mat4, unsigned int);
  /// @brief Set per instance transform attributes
  void SetTransform(const Mesh*, const std::vector<glm::mat4>&, unsigned int);
  virtual void Draw(const Mesh*);
  void DrawInstanced(const Mesh*, unsigned int);
};
class KUKI_API LitShader final : public Shader {
public:
  LitShader(const std::string&, const std::filesystem::path&, const std::filesystem::path&);
  void SetCamera(const Camera*) override;
  void SetLighting(const std::vector<const Light*>&);
  /// @brief Set lit material fallback attributes for a single instance
  void SetMaterialFallback(const Mesh*, LitFallbackData, unsigned int);
  /// @brief Set per instance lit material fallback attributes
  void SetMaterialFallback(const Mesh*, const std::vector<LitFallbackData>&, unsigned int);
  void Draw(const Mesh*) override;
};
class KUKI_API UnlitShader final : public Shader {
public:
  UnlitShader(const std::string&, const std::filesystem::path&, const std::filesystem::path&);
  /// @brief Set unlit material fallback attributes for a single instance
  void SetMaterialFallback(const Mesh*, UnlitFallbackData, unsigned int);
  /// @brief Set per instance unlit material fallback attributes
  void SetMaterialFallback(const Mesh*, const std::vector<UnlitFallbackData>&, unsigned int);
  void Draw(const Mesh*) override;
};
} // namespace kuki
