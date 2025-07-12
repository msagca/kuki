#pragma once
#include <glad/glad.h>
#include "texture_pool.hpp"
#include <kuki_engine_export.h>
namespace kuki {
class KUKI_ENGINE_API RenderbufferPool final : public Pool<TextureParams, GLuint> {
private:
  GLenum GetBaseFormat(GLenum);
protected:
  GLuint Allocate(const TextureParams&) override;
  void Reallocate(const TextureParams&, GLuint&) override;
public:
  ~RenderbufferPool() override;
  void Clear() override;
};
} // namespace kuki
