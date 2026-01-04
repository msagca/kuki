#pragma once
#include <bone_data.hpp>
#include <camera.hpp>
#include <glm/ext/matrix_float3x3.hpp>
#include <kuki_engine_export.h>
#include <shader.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
namespace kuki {
struct ShaderUniform {
  int location{};
  int size{};
  unsigned int type{};
};
class KUKI_ENGINE_API GLShaderBase : public Shader {
public:
  virtual ~GLShaderBase() = default;
  unsigned int id{};
  auto CacheLocations() -> void;
  auto SetTexture(const std::string &, const int) const -> void;
  auto SetUniform(const int, const float) const -> void;
  auto SetUniform(const int, const glm::mat4 &) const -> void;
  auto SetUniform(const int, const glm::vec3 &) const -> void;
  auto SetUniform(const int, const glm::vec4 &) const -> void;
  auto SetUniform(const int, const int) const -> void;
  auto SetUniform(const int, const int, const int *) const -> void;
  auto SetUniform(const int, const unsigned int) const -> void;
  auto SetUniform(const std::string &, const float) const -> void;
  auto SetUniform(const std::string &, const glm::mat4 &) const -> void;
  auto SetUniform(const std::string &, const glm::vec3 &) const -> void;
  auto SetUniform(const std::string &, const glm::vec4 &) const -> void;
  auto SetUniform(const std::string &, const int) const -> void;
  auto SetUniform(const std::string &, const int, const int *) const -> void;
  auto SetUniform(const std::string &, const unsigned int) const -> void;
  auto Use() const -> void;
  static auto Compile(const char *, const int) -> unsigned int;
  static auto GetTextureType(const unsigned int) -> unsigned int;
  static auto IsSamplerType(const unsigned int) -> bool;
protected:
  template <typename T>
  explicit GLShaderBase(std::in_place_type_t<T>);
  std::unordered_map<std::string, ShaderUniform> nameToUniform;
  std::unordered_map<std::string, std::vector<int>> nameToTextureUnit;
  std::unordered_set<int> cachedLocations;
};
template <typename T>
GLShaderBase::GLShaderBase(std::in_place_type_t<T> tag)
  : Shader(tag) {}
} // namespace kuki
