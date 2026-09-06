#include <format>
#include <gl_shader_base.hpp>
#include <glad/glad.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <shader_definitions.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
namespace kuki {
namespace {
/// @brief Splices the engine's shader definitions in below a shader's `#version` line.
///
/// GLSL will not accept anything before `#version`, and the driver takes the whole shader as one
/// string, so the only way to hand a shader a value the C++ decided is to edit the source on the
/// way past. Every GLSL shader in the engine opens with `#version 460 core` and none of them use
/// `#extension`, which has to come before any other statement, so the line after the first is
/// always a legal place for this.
///
/// The `#line` that follows puts the numbering back. Without it every diagnostic from every GLSL
/// shader would be reported as many lines further down as there are definitions, which is a poor
/// trade for a constant most shaders never read. `#line 2` says the line after it is line two of
/// the original file, which is what it was before the splice.
///
/// A shader that does not open with `#version` is handed back untouched rather than refused. There
/// are none in the engine, and the alternative is a splice that lands somewhere illegal and fails
/// as a compile error in a shader whose source, on disk, is perfectly correct.
auto WithDefinitions(const char *text) -> std::string {
  std::string source(text ? text : "");
  const auto firstLine = source.find('\n');
  if (firstLine == std::string::npos || !std::string_view(source).starts_with("#version"))
    return source;
  std::string spliced;
  for (const auto &definition : SHADER_DEFINITIONS)
    spliced += std::format("#define {} {}u\n", definition.name, definition.value);
  spliced += "#line 2\n";
  source.insert(firstLine + 1, spliced);
  return source;
}
} // namespace
auto GLShaderBase::CacheLocations() -> void {
  GLint params = 0;
  glGetProgramiv(id, GL_ACTIVE_UNIFORMS, &params);
  constexpr GLuint bufSize = 256;
  GLchar name[bufSize];
  GLenum type;
  GLint size;
  GLint textureUnit = 0;
  GLsizei length;
  for (auto i = 0; i < params; ++i) {
    glGetActiveUniform(id, i, bufSize, &length, &size, &type, name);
    const auto location = glGetUniformLocation(id, name);
    if (location >= 0) {
      nameToUniform[name] = {.location = location, .size = size, .type = type};
      cachedLocations.insert(location);
      if (IsSamplerType(type))
        for (auto j = 0; j < size; ++j)
          nameToTextureUnit[name].push_back(textureUnit++);
    }
  }
}
auto GLShaderBase::SetTexture(const std::string &name, const int texture) const -> void {
  if (auto it = nameToUniform.find(name); it != nameToUniform.end())
    if (auto it2 = nameToTextureUnit.find(name); it2 != nameToTextureUnit.end()) {
      const auto location = it->second.location;
      const auto samplerType = it->second.type;
      const auto textureType = GetTextureType(samplerType);
      for (const auto &textureUnit : it2->second) {
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(textureType, texture);
      }
      const auto size = it2->second.size();
      auto data = it2->second.data();
      SetUniform(location, size, data);
    }
}
auto GLShaderBase::SetUniform(const int location, const float value) const -> void {
  if (cachedLocations.find(location) == cachedLocations.end())
    return;
  glUniform1f(location, value);
}
auto GLShaderBase::SetUniform(const int location, const glm::mat4 &value) const -> void {
  if (cachedLocations.find(location) == cachedLocations.end())
    return;
  glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
}
auto GLShaderBase::SetUniform(const int location, const glm::vec2 &value) const -> void {
  if (cachedLocations.find(location) == cachedLocations.end())
    return;
  glUniform2fv(location, 1, glm::value_ptr(value));
}
auto GLShaderBase::SetUniform(const int location, const glm::vec3 &value) const -> void {
  if (cachedLocations.find(location) == cachedLocations.end())
    return;
  glUniform3fv(location, 1, glm::value_ptr(value));
}
auto GLShaderBase::SetUniform(const int location, const glm::vec4 &value) const -> void {
  if (cachedLocations.find(location) == cachedLocations.end())
    return;
  glUniform4fv(location, 1, glm::value_ptr(value));
}
auto GLShaderBase::SetUniform(const int location, const int count, const int *value) const -> void {
  if (cachedLocations.find(location) == cachedLocations.end())
    return;
  glUniform1iv(location, count, value);
}
auto GLShaderBase::SetUniform(const int location, const int value) const -> void {
  if (cachedLocations.find(location) == cachedLocations.end())
    return;
  glUniform1i(location, value);
}
auto GLShaderBase::SetUniform(const int location, const unsigned int value) const -> void {
  if (cachedLocations.find(location) == cachedLocations.end())
    return;
  glUniform1ui(location, value);
}
auto GLShaderBase::SetUniform(const std::string &name, const float value) const -> void {
  if (auto it = nameToUniform.find(name); it != nameToUniform.end())
    glUniform1f(it->second.location, value);
}
auto GLShaderBase::SetUniform(const std::string &name, const glm::mat4 &value) const -> void {
  if (auto it = nameToUniform.find(name); it != nameToUniform.end())
    glUniformMatrix4fv(it->second.location, 1, GL_FALSE, glm::value_ptr(value));
}
auto GLShaderBase::SetUniform(const std::string &name, const glm::vec2 &value) const -> void {
  if (auto it = nameToUniform.find(name); it != nameToUniform.end())
    glUniform2fv(it->second.location, 1, glm::value_ptr(value));
}
auto GLShaderBase::SetUniform(const std::string &name, const glm::vec3 &value) const -> void {
  if (auto it = nameToUniform.find(name); it != nameToUniform.end())
    glUniform3fv(it->second.location, 1, glm::value_ptr(value));
}
auto GLShaderBase::SetUniform(const std::string &name, const glm::vec4 &value) const -> void {
  if (auto it = nameToUniform.find(name); it != nameToUniform.end())
    glUniform4fv(it->second.location, 1, glm::value_ptr(value));
}
auto GLShaderBase::SetUniform(const std::string &name, const int count, const int *value) const -> void {
  if (auto it = nameToUniform.find(name); it != nameToUniform.end())
    glUniform1iv(it->second.location, count, value);
}
auto GLShaderBase::SetUniform(const std::string &name, const int value) const -> void {
  if (auto it = nameToUniform.find(name); it != nameToUniform.end())
    glUniform1i(it->second.location, value);
}
auto GLShaderBase::SetUniform(const std::string &name, const int count, const unsigned int *value) const -> void {
  if (auto it = nameToUniform.find(name); it != nameToUniform.end())
    glUniform1uiv(it->second.location, count, value);
}
auto GLShaderBase::SetUniform(const std::string &name, const unsigned int value) const -> void {
  if (auto it = nameToUniform.find(name); it != nameToUniform.end())
    glUniform1ui(it->second.location, value);
}
void GLShaderBase::Use() const {
  glUseProgram(id);
}
auto GLShaderBase::Compile(const char *text, const int type, const std::string &name) -> unsigned int {
  auto id = glCreateShader(type);
  const auto source = WithDefinitions(text);
  const auto *sourceText = source.c_str();
  glShaderSource(id, 1, &sourceText, nullptr);
  glCompileShader(id);
  int success;
  glGetShaderiv(id, GL_COMPILE_STATUS, &success);
  if (!success) {
    int logLength{};
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &logLength);
    std::string log(logLength, '\0');
    if (logLength > 0)
      glGetShaderInfoLog(id, logLength, nullptr, log.data());
    spdlog::error("[OpenGL] Failed to compile shader: {}\n{}", name, log);
  }
  return id;
}
auto GLShaderBase::GetTextureType(const unsigned int samplerType) -> unsigned int {
  switch (samplerType) {
  case GL_SAMPLER_1D:
  case GL_SAMPLER_1D_SHADOW:
    return GL_TEXTURE_1D;
  case GL_SAMPLER_2D:
  case GL_SAMPLER_2D_SHADOW:
    return GL_TEXTURE_2D;
  case GL_SAMPLER_3D:
    return GL_TEXTURE_3D;
  case GL_SAMPLER_CUBE:
  case GL_SAMPLER_CUBE_SHADOW:
    return GL_TEXTURE_CUBE_MAP;
  case GL_SAMPLER_1D_ARRAY:
  case GL_SAMPLER_1D_ARRAY_SHADOW:
    return GL_TEXTURE_1D_ARRAY;
  case GL_SAMPLER_2D_ARRAY:
  case GL_SAMPLER_2D_ARRAY_SHADOW:
    return GL_TEXTURE_2D_ARRAY;
  case GL_SAMPLER_BUFFER:
    return GL_TEXTURE_BUFFER;
  case GL_SAMPLER_2D_RECT:
  case GL_SAMPLER_2D_RECT_SHADOW:
    return GL_TEXTURE_RECTANGLE;
  case GL_SAMPLER_2D_MULTISAMPLE:
    return GL_TEXTURE_2D_MULTISAMPLE;
  case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
    return GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
  case GL_INT_SAMPLER_2D:
  case GL_UNSIGNED_INT_SAMPLER_2D:
    return GL_TEXTURE_2D;
  default:
    return 0;
  }
}
auto GLShaderBase::IsSamplerType(const unsigned int type) -> bool {
  switch (type) {
  case GL_SAMPLER_1D:
  case GL_SAMPLER_2D:
  case GL_SAMPLER_3D:
  case GL_SAMPLER_CUBE:
  case GL_SAMPLER_1D_SHADOW:
  case GL_SAMPLER_2D_SHADOW:
  case GL_SAMPLER_CUBE_SHADOW:
  case GL_SAMPLER_1D_ARRAY:
  case GL_SAMPLER_2D_ARRAY:
  case GL_SAMPLER_1D_ARRAY_SHADOW:
  case GL_SAMPLER_2D_ARRAY_SHADOW:
  case GL_SAMPLER_BUFFER:
  case GL_SAMPLER_2D_RECT:
  case GL_SAMPLER_2D_RECT_SHADOW:
  case GL_SAMPLER_2D_MULTISAMPLE:
  case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
  case GL_INT_SAMPLER_2D:
  case GL_UNSIGNED_INT_SAMPLER_2D:
    return true;
  default:
    return false;
  }
}
} // namespace kuki
