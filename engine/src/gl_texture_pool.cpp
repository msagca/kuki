#include <algorithm>
#include <gl_format.hpp>
#include <glad/glad.h>
#include <keyed_pool.hpp>
#include <target_description.hpp>
#include <gl_texture_pool.hpp>
namespace kuki {
auto GLTexturePool::Allocate(const TargetDescription &desc) -> unsigned int {
  unsigned int texture;
  glGenTextures(1, &texture);
  Reallocate(desc, texture);
  return texture;
}
auto GLTexturePool::Deallocate(unsigned int &texture) -> void {
  glDeleteTextures(1, &texture);
  texture = 0;
}
auto GLTexturePool::Reallocate(const TargetDescription &desc, unsigned int &texture) -> void {
  if (texture == 0)
    return;
  const auto format = TargetFormatToGL(desc.format);
  const auto target = desc.type == TargetType::Cubemap                              ? GL_TEXTURE_CUBE_MAP
                      : desc.type == TargetType::Texture2DArray                     ? GL_TEXTURE_2D_ARRAY
                      : desc.type == TargetType::Texture2DMulti || desc.samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE
                                                                                    : GL_TEXTURE_2D;
  const auto type = format.external == GL_DEPTH_COMPONENT ? GL_FLOAT : GL_UNSIGNED_BYTE;
  glBindTexture(target, texture);
  if (target == GL_TEXTURE_2D_MULTISAMPLE)
    glTexImage2DMultisample(target, desc.samples, format.internal, desc.width, desc.height, GL_TRUE);
  else {
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, desc.mipmaps > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    if (format.external == GL_DEPTH_COMPONENT) {
      glTexParameteri(target, GL_TEXTURE_SWIZZLE_G, GL_RED);
      glTexParameteri(target, GL_TEXTURE_SWIZZLE_B, GL_RED);
      glTexParameteri(target, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    }
    for (auto level = 0; level < desc.mipmaps; ++level) {
      const auto width = std::max(1, desc.width >> level);
      const auto height = std::max(1, desc.height >> level);
      if (target == GL_TEXTURE_CUBE_MAP)
        for (auto face = 0; face < 6; ++face)
          glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, format.internal, width, height, 0, format.external, type, nullptr);
      else if (target == GL_TEXTURE_2D_ARRAY)
        glTexImage3D(target, level, format.internal, width, height, desc.layers, 0, format.external, type, nullptr);
      else
        glTexImage2D(target, level, format.internal, width, height, 0, format.external, type, nullptr);
    }
  }
  glBindTexture(target, 0);
}
} // namespace kuki
