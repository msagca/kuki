#include <gl_renderer.hpp>
#include <keyed_pool.hpp>
#include <target_description.hpp>
#include <texture_pool.hpp>
//
#include <glad/glad.h>
namespace kuki {
TexturePool::~TexturePool() {
  Clear();
}
auto TexturePool::Allocate(const TargetDescription &desc) -> unsigned int {
  unsigned int texture;
  glGenTextures(1, &texture);
  Reallocate(desc, texture);
  return texture;
}
auto TexturePool::Clear() -> void {
  for (const auto &[params, textures] : pool)
    for (const auto &id : textures)
      glDeleteTextures(1, &id);
}
auto TexturePool::Reallocate(const TargetDescription &desc, unsigned int &texture) -> void {
  if (texture == 0)
    return;
  const auto &format = GLRenderer::TargetFormatToGL(desc.format);
  const auto target = desc.samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
  // TODO: handle cubemaps
  glBindTexture(target, texture);
  if (target == GL_TEXTURE_2D_MULTISAMPLE)
    glTexImage2DMultisample(target, desc.samples, format.internal, desc.width, desc.height, GL_TRUE);
  else {
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, desc.mipmaps > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    for (auto level = 0; level < desc.mipmaps; ++level) {
      auto width = std::max(1, desc.width >> level);
      auto height = std::max(1, desc.height >> level);
      glTexImage2D(target, level, format.internal, width, height, 0, format.external, GL_UNSIGNED_BYTE, nullptr);
    }
  }
  glBindTexture(target, 0);
}
} // namespace kuki
