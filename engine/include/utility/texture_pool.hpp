#pragma once
#include <glad/glad.h>
#include "pool.hpp"
#include <kuki_engine_export.h>
namespace kuki {
struct KUKI_ENGINE_API TextureParams {
  int width{1024};
  int height{1024};
  int target{GL_TEXTURE_2D};
  int format{GL_RGB16F};
  int samples{1};
  int mipmaps{1};
  TextureParams(int = 1024, int = 1024, int = GL_TEXTURE_2D, int = GL_RGB16F, int = 1, int = 1);
  bool operator==(const TextureParams&) const;
  struct Hash {
    size_t operator()(const TextureParams&) const;
  };
};
class KUKI_ENGINE_API TexturePool final : public Pool<TextureParams, GLuint> {
private:
  GLenum GetBaseFormat(GLenum);
protected:
  GLuint Allocate(const TextureParams&) override;
  void Reallocate(const TextureParams&, GLuint&) override;
public:
  ~TexturePool() override;
  void Clear() override;
};
} // namespace kuki
