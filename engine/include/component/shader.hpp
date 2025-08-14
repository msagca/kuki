#pragma once
#include "camera.hpp"
#include "component.hpp"
#include "light.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include <filesystem>
#include <glm/ext/matrix_float3x3.hpp>
#include <kuki_engine_export.h>
#include <string>
namespace kuki {
class KUKI_ENGINE_API IShader {
private:
  const std::string name;
protected:
  unsigned int id{0};
  std::string Read(const std::filesystem::path&);
  unsigned int Compile(const char*, int);
  void CacheLocations();
  std::unordered_map<std::string, int> locations;
public:
  IShader(const std::string&);
  virtual ~IShader() = default;
  const std::string& GetName() const;
  unsigned int GetId() const;
  void Use() const;
  void SetUniform(const std::string& name, const glm::mat4& value);
  void SetUniform(const std::string& name, const glm::vec3& value);
  void SetUniform(const std::string& name, const glm::vec4& value);
  void SetUniform(const std::string& name, float value);
  void SetUniform(const std::string& name, int value);
  void SetUniform(const std::string& name, unsigned int value);
  void SetUniform(int location, const glm::mat4& value);
  void SetUniform(int location, const glm::vec3& value);
  void SetUniform(int location, const glm::vec4& value);
  void SetUniform(int location, float value);
  void SetUniform(int location, int value);
  void SetUniform(int location, unsigned int value);
};
class KUKI_ENGINE_API ComputeShader : public IShader {
private:
  ComputeType type;
public:
  ComputeShader(const std::string&, const std::filesystem::path&, ComputeType = ComputeType::BRDF);
  ComputeType GetType() const;
};
class KUKI_ENGINE_API Shader : public IShader {
private:
  MaterialType type;
public:
  Shader(const std::string&, const std::filesystem::path&, const std::filesystem::path&, MaterialType = MaterialType::Unlit);
  MaterialType GetType() const;
  virtual void SetCamera(const Camera*);
  /// @brief Set material textures and enable texture units
  void SetMaterial(const Material*);
  /// @brief Set transform attributes for a single instance
  void SetTransform(const Mesh*, glm::mat4, unsigned int);
  /// @brief Set per instance transform attributes
  void SetTransform(const Mesh*, const std::vector<glm::mat4>&, unsigned int);
  virtual void Draw(const Mesh*);
  void DrawInstanced(const Mesh*, unsigned int);
};
class KUKI_ENGINE_API LitShader final : public Shader {
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
class KUKI_ENGINE_API UnlitShader final : public Shader {
public:
  UnlitShader(const std::string&, const std::filesystem::path&, const std::filesystem::path&);
  /// @brief Set unlit material fallback attributes for a single instance
  void SetMaterialFallback(const Mesh*, UnlitFallbackData, unsigned int);
  /// @brief Set per instance unlit material fallback attributes
  void SetMaterialFallback(const Mesh*, const std::vector<UnlitFallbackData>&, unsigned int);
  void Draw(const Mesh*) override;
};
} // namespace kuki
