#include <gl_format.hpp>
#include <glad/glad.h>
#include <target_description.hpp>
namespace kuki {
auto GLFormatToTarget(const unsigned int format) -> TargetFormat {
  switch (format) {
  case GL_R8:
    return TargetFormat::R8;
  case GL_RG8:
    return TargetFormat::RG8;
  case GL_RGB8:
    return TargetFormat::RGB8;
  case GL_R16F:
    return TargetFormat::R16;
  case GL_RG16F:
    return TargetFormat::RG16;
  case GL_RGB16F:
    return TargetFormat::RGB16;
  case GL_RGB32F:
    return TargetFormat::RGB32;
  case GL_RGBA8:
    return TargetFormat::RGBA8;
  case GL_RGBA16F:
    return TargetFormat::RGBA16;
  case GL_RGBA32F:
    return TargetFormat::RGBA32;
  case GL_DEPTH_COMPONENT:
    return TargetFormat::DEPTH;
  default:
    return TargetFormat::Unknown;
  }
}
auto TargetFormatToGL(const TargetFormat &format) -> GLFormat {
  switch (format) {
  case TargetFormat::R8:
    return {GL_RED, GL_R8};
  case TargetFormat::RG8:
    return {GL_RG, GL_RG8};
  case TargetFormat::RGB8:
    return {GL_RGB, GL_RGB8};
  case TargetFormat::R16:
    return {GL_RED, GL_R16F};
  case TargetFormat::RG16:
    return {GL_RG, GL_RG16F};
  case TargetFormat::RGB16:
    return {GL_RGB, GL_RGB16F};
  case TargetFormat::RGB32:
    return {GL_RGB, GL_RGB32F};
  case TargetFormat::RGBA8:
    return {GL_RGBA, GL_RGBA8};
  case TargetFormat::RGBA16:
    return {GL_RGBA, GL_RGBA16F};
  case TargetFormat::RGBA32:
    return {GL_RGBA, GL_RGBA32F};
  case TargetFormat::DEPTH:
    return {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT};
  default:
    return {};
  }
}
} // namespace kuki
