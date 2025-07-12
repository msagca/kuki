#include <utility/texture_pool.hpp>
#include <type_traits>
#include <utility/pool.hpp>
namespace kuki {
TextureParams::TextureParams(int width, int height, int target, int format, int samples, int mipmaps)
  : width(width), height(height), target(target), format(format), samples(samples), mipmaps(mipmaps) {}
bool TextureParams::operator==(const TextureParams& other) const {
  return width == other.width && height == other.height && target == other.target && format == other.format && samples == other.samples && mipmaps == other.mipmaps;
}
size_t TextureParams::Hash::operator()(const TextureParams& params) const {
  size_t h;
  h = std::hash<int>{}(params.width);
  h ^= std::hash<int>{}(params.height) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= std::hash<int>{}(params.target) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= std::hash<int>{}(params.format) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= std::hash<int>{}(params.samples) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= std::hash<int>{}(params.mipmaps) + 0x9e3779b9 + (h << 6) + (h >> 2);
  return h;
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
  glBindTexture(params.target, texture);
  if (params.target == GL_TEXTURE_2D_MULTISAMPLE)
    glTexImage2DMultisample(params.target, params.samples, params.format, params.width, params.height, GL_TRUE);
  else {
    glTexParameteri(params.target, GL_TEXTURE_MIN_FILTER, params.mipmaps > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(params.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(params.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    auto baseFormat = GetBaseFormat(params.format);
    if (params.target == GL_TEXTURE_CUBE_MAP) {
      glTexParameteri(params.target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
      for (auto i = 0; i < 6; ++i) {
        auto target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
        glTexImage2D(target, 0, params.format, params.width, params.height, 0, baseFormat, GL_FLOAT, nullptr);
      }
    } else
      glTexImage2D(params.target, 0, params.format, params.width, params.height, 0, baseFormat, GL_FLOAT, nullptr);
    if (params.mipmaps > 1)
      glGenerateMipmap(params.target);
  }
  glBindTexture(params.target, 0);
}
GLenum TexturePool::GetBaseFormat(GLenum internalFormat) {
  switch (internalFormat) {
  case GL_RGBA16F:
  case GL_RGBA8:
  case GL_RGBA:
    return GL_RGBA;
  case GL_RGB16F:
  case GL_RGB8:
  case GL_RGB:
    return GL_RGB;
  case GL_RG16F:
  case GL_RG8:
  case GL_RG:
    return GL_RG;
  case GL_R16F:
  case GL_R8:
  case GL_R:
    return GL_RED;
  case GL_DEPTH_COMPONENT:
  case GL_DEPTH_COMPONENT16:
  case GL_DEPTH_COMPONENT24:
  case GL_DEPTH_COMPONENT32F:
    return GL_DEPTH_COMPONENT;
  default:
    return GL_RGB;
  }
}
} // namespace kuki
