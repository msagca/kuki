#include <glad/glad.h>
#include <pool.hpp>
#include <texture_params.hpp>
#include <texture_pool.hpp>
namespace kuki {
TextureParams::TextureParams(int width, int height, int target, int format, int samples, int mipmaps)
  : width(width), height(height), target(target), format(format), samples(samples), mipmaps(mipmaps) {}
bool TextureParams::operator==(const TextureParams& other) const {
  return width == other.width && height == other.height && target == other.target && format == other.format && samples == other.samples && mipmaps == other.mipmaps;
}
TexturePool::~TexturePool() {
  Clear();
}
void TexturePool::Clear() {
  for (const auto& [params, textures] : pool)
    for (auto id : textures)
      glDeleteTextures(1, &id);
  Pool::Clear();
}
GLuint TexturePool::Allocate(const TextureParams& params) {
  GLuint texture;
  glGenTextures(1, &texture);
  Reallocate(params, texture);
  return texture;
}
void TexturePool::Reallocate(const TextureParams& params, GLuint& texture) {
  // FIXME: this creates immutable storage — resizing is not possible
  if (texture == 0)
    return;
  glBindTexture(params.target, texture);
  if (params.target == GL_TEXTURE_2D_MULTISAMPLE)
    glTexStorage2DMultisample(params.target, params.samples, params.format, params.width, params.height, GL_TRUE);
  else {
    glTexParameteri(params.target, GL_TEXTURE_MIN_FILTER, params.mipmaps > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(params.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(params.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (params.target == GL_TEXTURE_CUBE_MAP)
      glTexParameteri(params.target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexStorage2D(params.target, params.mipmaps, params.format, params.width, params.height);
  }
  glBindTexture(params.target, 0);
}
} // namespace kuki
