#include <gl_renderer.hpp>
#include <pool.hpp>
#include <texture_pool.hpp>
//
#include <glad/glad.h>
namespace kuki {
TexturePool::~TexturePool() {
  Clear();
}
auto TexturePool::Clear() -> void {
  for (const auto &[params, textures] : pool)
    for (auto id : textures)
      glDeleteTextures(1, &id);
}
auto TexturePool::Allocate(const TargetDescription &desc) -> unsigned int {
  unsigned int texture;
  glGenTextures(1, &texture);
  Reallocate(desc, texture);
  return texture;
}
auto TexturePool::Reallocate(const TargetDescription &desc, unsigned int &texture) -> void {
  // FIXME: this creates immutable storage — resizing is not possible
  if (texture == 0)
    return;
  const auto format = GLRenderer::TargetFormatToGL(desc.format);
  const auto target = GLRenderer::TargetTypeToGL(desc.target);
  glBindTexture(target, texture);
  if (target == GL_TEXTURE_2D_MULTISAMPLE)
    glTexStorage2DMultisample(target, desc.samples, format, desc.width, desc.height, GL_TRUE);
  else {
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, desc.mipmaps > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (target == GL_TEXTURE_CUBE_MAP)
      glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexStorage2D(target, desc.mipmaps, format, desc.width, desc.height);
  }
  glBindTexture(target, 0);
}
} // namespace kuki
