#pragma once
#include <bone_data.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <filesystem>
#include <glm/ext/matrix_float3x3.hpp>
#include <kuki_engine_export.h>
#include <light.hpp>
#include <material.hpp>
#include <mesh.hpp>
#include <span>
#include <string>
namespace kuki {
class RenderingSystem;
class KUKI_ENGINE_API IShader {
private:
  const std::string name;
protected:
  RenderingSystem& renderer;
  unsigned int id{0};
  std::string Read(const std::filesystem::path&);
  unsigned int Compile(const char*, int);
  void CacheLocations();
  std::unordered_map<std::string, int> locations;
public:
  IShader(const std::string&, RenderingSystem&);
  virtual ~IShader() = default;
  const std::string& GetName() const;
  unsigned int GetId() const;
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
class KUKI_ENGINE_API ComputeShader : public IShader {
private:
  ComputeType type;
public:
  ComputeShader(const std::string&, const std::filesystem::path&, RenderingSystem&, ComputeType = ComputeType::BRDF);
  ComputeType GetType() const;
};
class KUKI_ENGINE_API Shader : public IShader {
private:
  MaterialType type;
public:
  Shader(const std::string&, const std::filesystem::path&, const std::filesystem::path&, RenderingSystem&, MaterialType = MaterialType::Unlit);
  MaterialType GetType() const;
  virtual void SetCamera(const Camera*);
  /// @brief Set material textures and enable texture units
  void SetMaterial(const Material*);
  /// @brief Set transform attributes for a single instance
  void SetTransform(const Mesh*, const glm::mat4&, unsigned int);
  /// @brief Set transform attributes for multiple instances
  void SetTransform(const Mesh*, std::span<const glm::mat4>, unsigned int);
  void SetBoneTransforms(const BoneData);
  virtual void Draw(const Mesh*);
  void DrawInstanced(const Mesh*, unsigned int);
};
class KUKI_ENGINE_API LitShader final : public Shader {
public:
  LitShader(const std::string&, const std::filesystem::path&, const std::filesystem::path&, RenderingSystem&);
  void SetCamera(const Camera*) override;
  void SetLighting(const Light*);
  void SetLighting(std::span<const Light*>);
  /// @brief Set lit material fallback attributes for a single instance
  void SetMaterialFallback(const Mesh*, const LitFallbackData&, unsigned int);
  /// @brief Set lit material fallback attributes for multiple instances
  void SetMaterialFallback(const Mesh*, std::span<const LitFallbackData>, unsigned int);
  void Draw(const Mesh*) override;
};
class KUKI_ENGINE_API UnlitShader final : public Shader {
public:
  UnlitShader(const std::string&, const std::filesystem::path&, const std::filesystem::path&, RenderingSystem&);
  /// @brief Set unlit material fallback attributes for a single instance
  void SetMaterialFallback(const Mesh*, const UnlitFallbackData&, unsigned int);
  /// @brief Set unlit material fallback attributes for multiple instances
  void SetMaterialFallback(const Mesh*, std::span<const UnlitFallbackData>, unsigned int);
  void Draw(const Mesh*) override;
};
} // namespace kuki
